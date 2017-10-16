#!/bin/bash

ubuntu(){
	sudo apt update
	sudo apt install dh-autoreconf libcurl4-gnutls-dev libglib2.0-dev libelf-dev libdw-dev gcc check

	# compile telemetrics-client
	sh ./autogen.sh
	./configure
	make
	sudo make install
	# -------------------------

	# ------------------------- 
	sudo apt remove mcelog
	if [ ! -d mcelog/ ]; then
 	git clone https://github.com/andikleen/mcelog
 	cd mcelog && mkdir mcedir && cd mcedir 
 	git clone https://github.com/clearlinux-pkgs/mcelog
 	cd mcelog && cp 0001-Send-telemetry-record-on-MCE.patch ../..
 	cd ../.. 
 	patch -i 0001-Send-telemetry-record-on-MCE.patch
 	make
 	sudo make install
	fi
	# -------------------------

	sudo mkdir -p /etc/telemetrics/
	sudo cp /usr/local/share/defaults/telemetrics/telemetrics.conf /etc/telemetrics/telemetrics.conf
	sudo mkdir -p $(cat /etc/telemetrics/telemetrics.conf | grep spool_dir | cut -d"=" -f2)
	sudo mkdir -p /usr/local/var/
	sudo mkdir -p /etc/telemetrics/

	sudo echo "ubuntu-id" > /etc/telemetrics/opt-in-static-machine-id

	sudo mkdir -p /usr/local/var/lib/telemetry/  
	sudo echo "ubuntu-id" > /usr/local/var/lib/telemetry/machine_id
	sudo usermod -aG root telemetry
	echo "Installation finished"
}

fedora(){
	sudo yum update
	sudo yum install dh-autoreconf check* libcurl* glib* elfutils* libdw* gcc

	# compile telemetrics-client
	sh ./autogen.sh
	./configure
	make
	sudo make install
	# -------------------------

    # ------------------------- 
	sudo apt remove mcelog
	if [ ! -d mcelog/ ]; then
 	git clone https://github.com/andikleen/mcelog
 	cd mcelog && mkdir mcedir && cd mcedir 
 	git clone https://github.com/clearlinux-pkgs/mcelog
 	cd mcelog && cp 0001-Send-telemetry-record-on-MCE.patch ../..
 	cd ../.. 
 	patch -i 0001-Send-telemetry-record-on-MCE.patch
 	make
 	sudo make install
	fi
	# -------------------------


	sudo mkdir -p /etc/telemetrics/
	sudo cp /usr/local/share/defaults/telemetrics/telemetrics.conf /etc/telemetrics/telemetrics.conf
	sudo mkdir -p $(cat /etc/telemetrics/telemetrics.conf | grep spool_dir | cut -d"=" -f2)
	sudo mkdir -p /usr/local/var/lib/telemetry

	sudo echo "fedora-id" > /etc/telemetrics/opt-in-static-machine-id
    sudo mkdir -p /usr/local/var/lib/telemetry/
    sudo echo "fedora-id" > /usr/local/var/lib/telemetry/machine_id
    sudo usermod -aG root telemetry
	echo "Installation finished"
}

opensuse(){
	sudo zypper update
	sudo zypper install dh-autoreconf autoconf gcc check-devel libcurl-devel glib2-devel libelf-devel libdw-devel libtool
	sudo OCICLI http://software.opensuse.org/ymp/devel:tools:building/openSUSE_Leap_42.2/automake.ymp # This install automake 1.14

	# compile telemetrics-client
	sh ./autogen.sh
	./configure
	make
	sudo make install
	# -------------------------

	# 
	sudo zypper remove mcelog
	git clone https://github.com/andikleen/mcelog
	cd mcelog && mkdir  mcedir && cd $_ && git clone https://github.com/clearlinux-pkgs/mcelog && cd mcelog && cp 0001-Send-telemetry-record-on-MCE.patch ../..
	cd ../.. 
	patch -i 0001-Send-telemetry-record-on-MCE.patch
	make
	sudo make install
	# -------------------------


	sudo mkdir -p /etc/telemetrics/
	sudo cp /usr/local/share/defaults/telemetrics/telemetrics.conf /etc/telemetrics/telemetrics.conf
	sudo mkdir -p $(cat /etc/telemetrics/telemetrics.conf | grep spool_dir | cut -d"=" -f2)
	sudo mkdir -p /usr/local/var/lib/telemetry
	cp $(find / -name "libtelemetry.so.3" | head -n 1) /usr/lib/ && cp $(find / -name "libtelemetry.so.3" | head -n 1) /usr/lib64/


	sudo echo "suse-id" > /etc/telemetrics/opt-in-static-machine-id
    sudo mkdir -p /usr/local/var/lib/telemetry/
    sudo echo "suse-id" > /usr/local/var/lib/telemetry/machine_id	
	sudo usermod -aG root telemetry
	echo "Installation finished"
}

usage(){
 printf "Install telemtrics client for multiple distros"
 printf "\n"
 printf "%15s \n" "install.sh ubuntu"
 printf "%15s \n" "install.sh fedora"
 printf "%15s \n" "install.sh opensuse"

}

SUBCOMMAND=$1

case $SUBCOMMAND in
	ubuntu) 
	ubuntu ;;
	fedora)
	fedora ;;
	opensuse)
	opensuse ;;
	*)
	notice "Unknown command passed"
	usage ;;
esac

exit 0
