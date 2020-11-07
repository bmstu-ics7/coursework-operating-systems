#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/usb/input.h>

#define DRIVER_NAME    "keyboard_tablet"
#define DRIVER_AUTHOR  "Alexander Stepanov"
#define DRIVER_DESC    "Simulate table like a keyboard."
#define DRIVER_LICENSE "GPL"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);

#define ID_VENDOR_TABLET  0x056a /* Wacom Co. */
#define ID_PRODUCT_TABLET 0x0301 /* Ltd CTL-671 */

static int tablet_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    return 0;
}

static void tablet_disconnect(struct usb_interface *interface)
{
}

static struct usb_device_id tablet_table [] = {
    { USB_DEVICE(ID_VENDOR_TABLET, ID_PRODUCT_TABLET) },
    { },
};

MODULE_DEVICE_TABLE(usb, tablet_table);

static struct usb_driver tablet_driver = {
    .name       = "keyboard_tablet",
    .probe      = tablet_probe,
    .disconnect = tablet_disconnect,
    .id_table   = tablet_table,
};

static int __init keyboard_tablet_init(void)
{
    int result = usb_register(&tablet_driver);

    if (result < 0)
    {
        printk(KERN_ERR "[%s] usb register error", DRIVER_NAME);
        return result;
    }

    printk(KERN_INFO "[%s] module loaded\n", DRIVER_NAME);
    return 0;
}

static void __exit keyboard_tablet_exit(void)
{
    usb_deregister(&tablet_driver);
    printk(KERN_INFO "[%s] module unloaded\n", DRIVER_NAME);
}

module_init(keyboard_tablet_init);
module_exit(keyboard_tablet_exit);
