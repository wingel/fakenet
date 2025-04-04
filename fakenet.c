// SPDX-License-Identifier: GPL-2.0-or-later
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/mdio.h>
#include <linux/phy_fixed.h>
#include <linux/ioport.h>
#include <linux/pci.h>

#define DRV_NAME "fakenet"
#define DRV_VERSION "1.0"

static int nr_devices = 1;
module_param(nr_devices, int, 0444);
MODULE_PARM_DESC(nr_devices, "Number of fake network devices");

static struct net_device **fakenet_devs;

static void fakenet_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	strscpy(info->driver, DRV_NAME, sizeof(info->driver));
	strscpy(info->version, DRV_VERSION, sizeof(info->version));
}

static u32 fakenet_get_link(struct net_device *dev)
{
	return netif_carrier_ok(dev);
}

static const struct ethtool_ops fakenet_ethtool_ops = {
	.get_drvinfo = fakenet_get_drvinfo,
	.get_link = fakenet_get_link,
};

static int fakenet_open(struct net_device *dev)
{
	netif_start_queue(dev);
	netif_carrier_on(dev);
	return 0;
}

static int fakenet_stop(struct net_device *dev)
{
	netif_stop_queue(dev);
	netif_carrier_off(dev);
	return 0;
}

static netdev_tx_t fakenet_xmit(struct sk_buff *skb, struct net_device *dev)
{
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}

static const struct net_device_ops fakenet_ops = {
	.ndo_open = fakenet_open,
	.ndo_stop = fakenet_stop,
	.ndo_start_xmit = fakenet_xmit,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_validate_addr = eth_validate_addr,
};

static void fakenet_setup(struct net_device *dev)
{
	u8 dev_addr[ETH_ALEN];

	ether_setup(dev);

	dev->netdev_ops = &fakenet_ops;
	dev->ethtool_ops = &fakenet_ethtool_ops;

	// We don't need locking since we don't do nothing
	dev->features |= NETIF_F_LLTX;

	eth_random_addr(dev_addr);
	eth_hw_addr_set(dev, dev_addr);
}

static void fakenet_free(void) {
	int i;

	for (i = 0; i < nr_devices; i++) {
		if (fakenet_devs[i]) {
			struct net_device *fakenet_dev = fakenet_devs[i];
			unregister_netdev(fakenet_dev);
			free_netdev(fakenet_dev);
		}
	}

	kfree(fakenet_devs);
}

static int __init fakenet_init(void)
{
	struct pci_dev *pdev;
	int i;
	int err;

	if (nr_devices <= 0) {
		pr_err(DRV_NAME ": invalid nr_devices value: %d\n",
		       nr_devices);
		return -EINVAL;
	}

	fakenet_devs = kcalloc(nr_devices, sizeof(struct net_device *),
			       GFP_KERNEL);
	if (!fakenet_devs) {
		pr_err(DRV_NAME ": failed to allocate device pointers\n");
		return -ENOMEM;
	}

	// Get the first PCI device in the system, this is later used
	// as the device the net device is connected to
	pdev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, 0);

	for (i = 0; i < nr_devices; i++) {
		struct net_device *fakenet_dev =
			alloc_netdev(0, "eth%d", NET_NAME_ENUM, fakenet_setup);

		if (!fakenet_dev) {
			pr_err(DRV_NAME ": failed to allocate net_device %d\n", i);
			err = -ENOMEM;
			goto err_alloc;
		}

		if (pdev)
			SET_NETDEV_DEV(fakenet_dev, &pdev->dev);
		fakenet_dev->dev_port = i;

		err = register_netdev(fakenet_dev);
		if (err) {
			pr_err(DRV_NAME ": failed to register net_device %d: %d\n", i, err);
			free_netdev(fakenet_dev);
			goto err_alloc;
		}

		fakenet_devs[i] = fakenet_dev;
	}
      
	return 0;

err_alloc:
	fakenet_free();
	return err;
}

static void __exit fakenet_exit(void)
{
	fakenet_free();
}

module_init(fakenet_init);
module_exit(fakenet_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dummy Network Driver");
MODULE_AUTHOR("Christer Weinigel");
