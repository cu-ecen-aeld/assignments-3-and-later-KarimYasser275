#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
BUSYBOX_REPO=git://busybox.net/busybox.git
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE="aarch64-none-linux-gnu-"

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}


echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
echo "Creating rootfs directory at ${OUTDIR}/rootfs"
sudo mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
sudo mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
sudo mkdir -p usr/bin usr/lib usr/sbin
sudo mkdir -p var/log
echo "[DEBUG] rootfs directory created at ${OUTDIR}/rootfs"

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone  ${BUSYBOX_REPO}
    cd busybox
    echo "Checking out version ${BUSYBOX_VERSION}"
    git checkout ${BUSYBOX_VERSION}
    
else
    cd busybox
    echo "Checking out version ${BUSYBOX_VERSION}"
    git checkout ${BUSYBOX_VERSION}
fi

# TODO: Make and install busybox
    echo "[DEBUG] 2"
    # TODO:  Configure busybox
    sudo make distclean
    sudo make defconfig 

    sudo make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install 
    echo "[DEBUG] 3"

echo "Library dependencies"
cd "${OUTDIR}/rootfs"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs

cp /home/karim/x-tools/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib
cp /home/karim/x-tools/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64
cp /home/karim/x-tools/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64
cp /home/karim/x-tools/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64

# TODO: Make device nodes
mknod ${OUTDIR}/rootfs/dev/null c 1 3
mknod ${OUTDIR}/rootfs/dev/tty c 5 1
# TODO: Clean and build the writer utility
cd "${FINDER_APP_DIR}"
make clean
make CROSS_COMPILE=${CROSS_COMPILE}
echo | file "${FINDER_APP_DIR}/writer"

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp ${FINDER_APP_DIR}/writer ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home/
cp -r ${FINDER_APP_DIR}/conf/ ${OUTDIR}/rootfs/home/
cp ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home/

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    # Perform Deep cleaning
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    # Use default configurations
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    # Build the kernel
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    # Build the device tree
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
# Copying image to output directory
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

# TODO: Chown the root directory

# TODO: Create initramfs.cpio.gz
cd "${OUTDIR}/rootfs"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
gzip ${OUTDIR}/initramfs.cpio

