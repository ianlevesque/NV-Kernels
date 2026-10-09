// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/* Experimental powered-HCA retention. The coordinator must drain all work. */
#include <linux/dmi.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/suspend.h>
#include <linux/xarray.h>

static bool spark_retain_rdma;
module_param_named(spark_retain_rdma, spark_retain_rdma, bool, 0444);
MODULE_PARM_DESC(spark_retain_rdma,
	"Experimental: retain powered ConnectX-7 contexts in drained DGX Spark s2idle (default off)");
static DEFINE_XARRAY(spark_retained_devices);

int mlx5_spark_pm_suspend(struct pci_dev *pdev, pm_message_t state);
bool mlx5_spark_pm_resume(struct pci_dev *pdev);

/* Positive: use the ordinary unload path. Zero: retain. Negative: abort. */
int mlx5_spark_pm_suspend(struct pci_dev *pdev, pm_message_t state)
{
	void *entry;

	if (!spark_retain_rdma || state.event != PM_EVENT_SUSPEND ||
	    pm_suspend_target_state != PM_SUSPEND_TO_IDLE ||
	    pdev->device != 0x1021 ||
	    !(dmi_match(DMI_PRODUCT_NAME, "NVIDIA_DGX_Spark") ||
	      (dmi_match(DMI_SYS_VENDOR, "LENOVO") &&
	       dmi_match(DMI_PRODUCT_FAMILY, "DGX Spark"))))
		return 1;

	entry = xa_store(&spark_retained_devices, (unsigned long)pdev,
			 xa_mk_value(1), GFP_KERNEL);
	if (xa_is_err(entry))
		return xa_err(entry);
	dev_info(&pdev->dev, "Retaining powered ConnectX-7 contexts for DGX Spark s2idle\n");
	return 0;
}

/* Also handles failed-suspend unwind without unloading a retained device. */
bool mlx5_spark_pm_resume(struct pci_dev *pdev)
{
	if (!xa_erase(&spark_retained_devices, (unsigned long)pdev))
		return false;
	dev_info(&pdev->dev, "Resuming retained ConnectX-7 contexts after DGX Spark s2idle\n");
	return true;
}
