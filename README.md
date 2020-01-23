Based on broadcom official repository: https://github.com/Broadcom/stblinux-3.14/commit/2a47ae20e42328fe3106dd96cbdfda4058383893

Build:

  - wget --no-check-certificate -t6 -T20 -c https://github.com/Broadcom/stbgcc-6.3/releases/download/stbgcc-6.3-1.2/stbgcc-6.3-1.2.i386.tar.bz2 -P /opt/toolchains
  - tar -xvjf /opt/toolchains/stbgcc-6.3-1.2.i386.tar.bz2 -C /opt/toolchains
  - export ARCH=arm
  - export PATH=/opt/toolchains/stbgcc-6.3-1.2/bin:$PATH
  - export CROSS_COMPILE=arm-linux-gnu-
  - make defaults-7445d0
  - make images -j8
  - cd images
  - tar -cvf vmlinuz-initrd_vuultimo4k_$(date +%Y%m%d).tar vmlinuz-initrd-7445d0
  - gzip vmlinuz-initrd_vuultimo4k_$(date +%Y%m%d).tar

