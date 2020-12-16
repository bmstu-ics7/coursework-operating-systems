#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/usb/input.h>
#include <linux/slab.h>

#define DRIVER_NAME    "keyboard_tablet"
#define DRIVER_AUTHOR  "Alexander Stepanov"
#define DRIVER_DESC    "Simulate table like a keyboard."
#define DRIVER_LICENSE "GPL"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);

#define ID_VENDOR_TABLET  0x056a /* Wacom Co. */
#define ID_PRODUCT_TABLET 0x0301 /* Ltd CTL-671 */

#define USB_PACKET_LEN  10
#define WHEEL_THRESHOLD 4

#define MAX_X 1920
#define MAX_Y 1080

#define MAX_VALUE 0x7F

#define X_FACTOR (MAX_X / MAX_VALUE + 1)
#define Y_FACTOR (MAX_Y / MAX_VALUE + 1)

struct tablet_features {
    int pkg_len;
};

struct tablet {
    unsigned char     *data;
    dma_addr_t         data_dma;
    struct input_dev  *input_dev;
    struct usb_device *usb_dev;
    struct urb        *irq;
    int                old_wheel_pos;
    char               phys[32];
};

typedef struct tablet tablet_t;

static bool pen_enter;

static void tablet_irq(struct urb *urb) {
    int retval;
    u16 x, y;
    tablet_t *tablet = urb->context;
    unsigned char *data = tablet->data;

    if (urb->status != 0) {
        printk(KERN_ERR "%s: %s - urb status is %d\n", DRIVER_NAME, __func__, urb->status);
        return;
    }

    switch(data[1]) {
        case 0xf1:
            if (!pen_enter) {
                x = data[3] * X_FACTOR;
                y = data[5] * Y_FACTOR;

                printk(KERN_INFO "%s: pen enters %d %d\n", DRIVER_NAME, x, y);
                pen_enter = true;
            }
            break;
        case 0xf0:
            if (pen_enter) {
                printk(KERN_INFO "%s: pen leaves\n", DRIVER_NAME);
                pen_enter = false;
            }
            break;
        default:
            break;
    }

    retval = usb_submit_urb (urb, GFP_ATOMIC);
    if (retval)
        printk(KERN_ERR "%s: %s - usb_submit_urb failed with result %d\n", DRIVER_NAME, __func__, retval);
}

static int tablet_open(struct input_dev *dev) {
    tablet_t *tablet = input_get_drvdata(dev);

    tablet->old_wheel_pos = -WHEEL_THRESHOLD - 1;
    tablet->irq->dev = tablet->usb_dev;
    if (usb_submit_urb(tablet->irq, GFP_KERNEL))
        return -EIO;

    return 0;
}

static void tablet_close(struct input_dev *dev) {
    tablet_t *tablet = input_get_drvdata(dev);
    usb_kill_urb(tablet->irq);
}

static int tablet_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    struct usb_device *usb_device = interface_to_usbdev(interface);
    tablet_t *tablet;
    struct input_dev *input_dev;
    struct usb_endpoint_descriptor *endpoint;
    int error = -ENOMEM;

    printk(KERN_INFO "%s: probe checking tablet\n", DRIVER_NAME);

    tablet = kzalloc(sizeof(tablet_t), GFP_KERNEL);
    input_dev = input_allocate_device();
    if (!tablet || !input_dev) {
        input_free_device(input_dev);
        kfree(tablet);

        printk(KERN_ERR "%s: error when allocate device\n", DRIVER_NAME);
        return error;
    }

    tablet->data = (unsigned char *)usb_alloc_coherent(usb_device, USB_PACKET_LEN, GFP_KERNEL, &tablet->data_dma);
    if (!tablet->data) {
        input_free_device(input_dev);
        kfree(tablet);

        printk(KERN_ERR "%s: error when allocate coherent\n", DRIVER_NAME);
        return error;
    }

    tablet->irq = usb_alloc_urb(0, GFP_KERNEL);
    if (!tablet->irq) {
        usb_free_coherent(usb_device, USB_PACKET_LEN, tablet->data, tablet->data_dma);
        input_free_device(input_dev);
        kfree(tablet);

        printk(KERN_ERR "%s: error when allocate urb\n", DRIVER_NAME);
        return error;
    }

    tablet->usb_dev = usb_device;
    tablet->input_dev = input_dev;

    usb_make_path(usb_device, tablet->phys, sizeof(tablet->phys));
    strlcat(tablet->phys, "/input0", sizeof(tablet->phys));

    input_dev->name = DRIVER_NAME;
    input_dev->phys = tablet->phys;
    usb_to_input_id(usb_device, &input_dev->id);
    input_dev->dev.parent = &interface->dev;

    input_set_drvdata(input_dev, tablet);

    input_dev->open = tablet_open;
    input_dev->close = tablet_close;

    endpoint = &interface->cur_altsetting->endpoint[0].desc;

    usb_fill_int_urb(
        tablet->irq, usb_device,
        usb_rcvintpipe(usb_device, endpoint->bEndpointAddress),
        tablet->data, USB_PACKET_LEN,
        tablet_irq, tablet, endpoint->bInterval
    );

    usb_submit_urb(tablet->irq, GFP_ATOMIC);

    tablet->irq->transfer_dma = tablet->data_dma;
    tablet->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    error = input_register_device(tablet->input_dev);
    if (error) {
        usb_free_urb(tablet->irq);
        usb_free_coherent(usb_device, USB_PACKET_LEN, tablet->data, tablet->data_dma);
        input_free_device(input_dev);
        kfree(tablet);

        printk(KERN_ERR "%s: error when register device\n", DRIVER_NAME);
        return error;
    }

    usb_set_intfdata(interface, tablet);

    pen_enter = false;
    printk(KERN_INFO "%s: device is conected\n", DRIVER_NAME);

    return 0;
}

static void tablet_disconnect(struct usb_interface *interface) {
    tablet_t *tablet = usb_get_intfdata(interface);
    usb_set_intfdata(interface, NULL);

    if (tablet) {
        usb_kill_urb(tablet->irq);
        input_unregister_device(tablet->input_dev);
        usb_free_urb(tablet->irq);
        usb_free_coherent(interface_to_usbdev(interface), USB_PACKET_LEN, tablet->data, tablet->data_dma);
        kfree(tablet);

        printk(KERN_INFO "%s: device was disconected\n", DRIVER_NAME);
    }
}

static struct usb_device_id tablet_table [] = {
    { USB_DEVICE(ID_VENDOR_TABLET, ID_PRODUCT_TABLET) },
    { },
};

MODULE_DEVICE_TABLE(usb, tablet_table);

static struct usb_driver tablet_driver = {
    .name       = DRIVER_NAME,
    .probe      = tablet_probe,
    .disconnect = tablet_disconnect,
    .id_table   = tablet_table,
};

static int __init keyboard_tablet_init(void) {
    int result = usb_register(&tablet_driver);

    if (result < 0) {
        printk(KERN_ERR "%s: usb register error\n", DRIVER_NAME);
        return result;
    }

    printk(KERN_INFO "%s: module loaded\n", DRIVER_NAME);
    return 0;
}

static void __exit keyboard_tablet_exit(void) {
    usb_deregister(&tablet_driver);
    printk(KERN_INFO "%s: module unloaded\n", DRIVER_NAME);
}

module_init(keyboard_tablet_init);
module_exit(keyboard_tablet_exit);
