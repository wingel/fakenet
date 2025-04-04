# fakenet - a fake network device

A fake network device driver which supports ethtool and is linked to a
PCI device.  The PCI device it will be linked to is the first PCI
device in the system, usually the host bridge.

## Install driver

```
sudo dkms install .
```

If the sources are not already in /usr/src/fakenet-$PACKAGE_VERSION
the sources will be copied to that location.  So if this code is
checked out elswhere this directory can be removed afterwards.

## Use driver

```
sudo modprobe fakenet
```

If you want more than one device, use the nr_devices module parameter

```
sudo modprobe fakenet nr_devices=2
```

## Uninstall driver

```
sudo dkms remove fakenet/$PACKAGE_VERSION
```
