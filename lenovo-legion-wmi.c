#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/sysctl.h>
#include <linux/wmi.h>

#define  LENOVO_LEGION_WMI_VER "1.0"

MODULE_AUTHOR("Andy Bao <kernel@andybao.me>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Lenovo Legion WMI Driver");

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define  LENOVO_LEGION_WMI_GAME_ZONE_GUID "887B54E3-DDDC-4B2C-8B88-68A26A8835D0"
#define  LENOVO_LEGION_WMI_GET_GSYNC_STATUS_MID 41
#define  LENOVO_LEGION_WMI_SET_GSYNC_STATUS_MID 42

struct llwmi_basic_args {
	u8 arg;
};

static int llwmi_get_hybrid_graphics(u32 *enabled)
{
    struct acpi_buffer input = { 0, NULL };
    struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
    union acpi_object *obj;
    acpi_status status;

    status = wmi_evaluate_method(LENOVO_LEGION_WMI_GAME_ZONE_GUID, 0, 
            LENOVO_LEGION_WMI_GET_GSYNC_STATUS_MID, &input, &output);
    if (ACPI_FAILURE(status))
        return -EIO;

    obj = (union acpi_object *)output.pointer;
    if (obj && obj->type == ACPI_TYPE_INTEGER && enabled)
        *enabled = !(u32)obj->integer.value;

    kfree(obj);
    return 0;
}

static int llwmi_set_hybrid_graphics(u32 enabled)
{
    struct llwmi_basic_args args = { !enabled };
    struct acpi_buffer input = { (acpi_size)sizeof(args), &args };
    acpi_status status;

    status = wmi_evaluate_method(LENOVO_LEGION_WMI_GAME_ZONE_GUID, 0, 
            LENOVO_LEGION_WMI_SET_GSYNC_STATUS_MID, &input, NULL);
    if (ACPI_FAILURE(status))
        return -EIO;

    return 0;
}

static DEFINE_MUTEX(llwmi_update_mutex);

/*
 * Set via /proc/sys/kernel/lenovo_legion_wmi/hybrid_graphics
 */
unsigned int __read_mostly sysctl_llwmi_hybrid_graphics_status;

static int llwmi_hybrid_graphics_update_handler(struct ctl_table *table, int write,
        void *buffer, size_t *lenp, loff_t *ppos)
{
    unsigned int old_sysctl;
    int ret;

    mutex_lock(&llwmi_update_mutex);

    old_sysctl = sysctl_llwmi_hybrid_graphics_status;
    ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);

    if (!ret && write && old_sysctl != sysctl_llwmi_hybrid_graphics_status) {
        pr_info("hybrid_graphics: state after reboot will be: %d", sysctl_llwmi_hybrid_graphics_status);
        int status = llwmi_set_hybrid_graphics(sysctl_llwmi_hybrid_graphics_status);
        pr_info("hybrid_graphics: operation returned: %d", status);
    }

    mutex_unlock(&llwmi_update_mutex);

    return ret;
}

static struct ctl_table llwmi_table[] = {
    /**
     * Controls whether or not the system should use hybrid graphics or discrete graphics.
     * Changes will be applied on the next reboot.
     **/
    {
        .procname	= "hybrid_graphics",
        .data		= &sysctl_llwmi_hybrid_graphics_status,
        .maxlen		= sizeof(unsigned int),
        .mode		= 0644,
        .proc_handler	= llwmi_hybrid_graphics_update_handler,
        .extra1		= SYSCTL_ZERO,
        .extra2		= SYSCTL_ONE,
    },
    {}
};

static struct ctl_table llwmi_child_table[] = {
    {
        .procname	= "lenovo_legion_wmi",
        .mode		= 0555,
        .child		= llwmi_table,
    },
    {}
};

static struct ctl_table llwmi_root_table[] = {
    {
        .procname	= "kernel",
        .mode		= 0555,
        .child		= llwmi_child_table,
    },
    {}
};

static struct ctl_table_header *llwmi_sysctl_header;

static int setup_sysctl(void) {
    mutex_lock(&llwmi_update_mutex);

    int status = llwmi_get_hybrid_graphics(&sysctl_llwmi_hybrid_graphics_status);
    pr_info("hybrid_graphics: lookup returned %d, hybrid graphic status is: %d", status, sysctl_llwmi_hybrid_graphics_status);

    llwmi_sysctl_header = register_sysctl_table(llwmi_root_table);
    if (!llwmi_sysctl_header) {
        mutex_unlock(&llwmi_update_mutex);
        return -ENOMEM;
    }
    mutex_unlock(&llwmi_update_mutex);
    return 0;
}
static int teardown_sysctl(void) {
    mutex_lock(&llwmi_update_mutex);
    if (llwmi_sysctl_header) {
        unregister_sysctl_table(llwmi_sysctl_header);
        llwmi_sysctl_header = NULL;
    }
    mutex_unlock(&llwmi_update_mutex);
    return 0;
}

static int __init lenovo_legion_wmi_init(void)
{
    pr_info("loading version: %s", LENOVO_LEGION_WMI_VER);

    setup_sysctl();

    pr_info("loaded");
    return 0;
}

static void __exit lenovo_legion_wmi_cleanup(void)
{
    teardown_sysctl();

    pr_info("unloaded\n");
}

module_init(lenovo_legion_wmi_init);
module_exit(lenovo_legion_wmi_cleanup);
MODULE_VERSION(LENOVO_LEGION_WMI_VER);
