#!/bin/sh

echo=`which echo`

if [ `id -u` != 0 ]; then
	$echo "Run this as root! sudo should do the trick."
	exit
fi

if [ -f /etc/arch-release ]; then
	$echo -e "\e[1;31mArch Linux detected!\e[0m"
	DEPS=`pacman -T apr apr-util base-devel boost boost-libs c-ares cmake curl db dbus-glib expat fmodex fontconfig freealut freetype2 gperftools glib2 gstreamer0.10 gtk2 hunspell libjpeg-turbo libogg libpng libvorbis openal openssl pcre qt qtwebkit sdl zlib | sed -e 's/\n/ /g'`
	if [ -z "${DEPS}" ]; then
		$echo "Dependencies already installed."
	else
		pacman -S --asdeps ${DEPS}
	fi
elif [ -f /etc/lsb-release ] || [ -f /etc/debian_version ]; then
	$echo -e "\e[1;31mDebian/Ubuntu Linux detected!\e[0m"
	DEPS="libapr1-dev libaprutil1-dev build-essential libboost-dev libc-ares-dev cmake libcurl4-openssl-dev libdb-dev libdbus-glib-1-dev libexpat1-dev fontconfig libalut-dev libfreetype6-dev libgoogle-perftools-dev libglib2.0-dev libgstreamer-plugins-base0.10-dev libgtk2.0-dev libhunspell-dev libjpeg-turbo8-dev libogg-dev libpng12-dev libvorbis-dev libopenal-dev libssl-dev libpcre3-dev libqtwebkit-dev libsdl1.2-dev"
	for dep in $DEPS; do
		DQ="dpkg-query -f \${Status} -W $dep"
		DQ=`$DQ 2>/dev/null`
		if [ -z "$DQ" -o "$DQ" != "install ok installed" -a "$DQ" != "install ok installedinstall ok installed" ]; then
		#dpkg-query string empty, dpkg has yet to meet this package; or status is not installed and we don't have two archs of this package installed
			deps="$deps $dep"
		else $echo "Dependency, $dep, already installed."
		fi
	done
	if [ `uname -m` = "x86_64" ]; then
		DEPS="$deps"
		deps=""
		for dep in $DEPS; do
			DQ="dpkg-query -f \${Status} -W $dep:i386"
			DQ=`$DQ 2>/dev/null`
			if [ -z "$DQ" -o "$DQ" != "install ok installed" ]; then
			#dpkg-query string empty, dpkg has yet to meet this package; or status is not installed
				deps="$deps $dep"
			else $echo "Dependency, $dep, already installed as a 32-bit package."
			fi
		done
		DQ="dpkg-query -f \${Status} -W ia32-libs"
		DQ=`$DQ 2>/dev/null`
		if [ -z "$DQ" -o "$DQ" != "install ok installed" ]; then
		#dpkg-query string empty, dpkg has yet to meet ia32-libs, or it is not installed
			deps="ia32-libs $deps"
		else $echo "32-bit compatibility package already installed."
		fi
	fi
	if [ -n "$deps" ]; then
		#$echo $deps #Uncomment this for output of packages we've decided to install because of this script, not apt-get's ideas.
		apt-get install $deps #Not apt-get -y, that might clobber packages.
	fi
	$echo -e "If you want FMOD Ex for sound, please go the the \"Compiling with FMOD Ex\" section of \e[0;34mhttps://sites.google.com/site/singularityviewer/kb/build-linux\e[0m and follow the directions provided."
else
	$echo "Unsupported operating system."
fi
