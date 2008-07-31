# Copyright 1999-2008 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/www/viewcvs.gentoo.org/raw_cvs/gentoo-x86/sys-power/nut/nut-2.2.2.ebuild,v 1.5 2008/07/05 15:39:35 mr_bones_ Exp $

inherit eutils fixheadtails autotools

MY_P="${P/_/-}"

DESCRIPTION="Network-UPS Tools"
HOMEPAGE="http://www.networkupstools.org/"
# Nut mirrors are presently broken
SRC_URI="http://random.networkupstools.org/source/${PV%.*}/${MY_P}.tar.gz"

S="${WORKDIR}/${MY_P}"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~alpha ~amd64 ~ppc ~sparc ~x86 ~x86-fbsd"
IUSE="cgi snmp usb ssl hal xml"

RDEPEND="cgi? ( >=media-libs/gd-2 )
		snmp? ( net-analyzer/net-snmp )
		usb? ( >=dev-libs/libusb-0.1.12 )
		hal? ( >=sys-apps/hal-0.5.9.1 )
		ssl? ( dev-libs/openssl )
		xml? ( >=net-misc/neon-0.25.0 )
		>=sys-fs/udev-114"
DEPEND="$RDEPEND
		>=sys-apps/sed-4
		>=sys-devel/autoconf-2.58"

# public files should be 644 root:root
NUT_PUBLIC_FILES="/etc/nut/{{ups,upssched}.conf}"
# private files should be 640 root:nut - readable by nut, writeable by root,
NUT_PRIVATE_FILES="/etc/nut/{upsd.conf,upsd.users,upsmon.conf}"
# public files should be 644 root:root, only installed if USE=cgi
NUT_CGI_FILES="/etc/nut/{{hosts,upsset}.conf,upsstats{,-single}.html}"

pkg_setup() {
	if use cgi && ! built_with_use media-libs/gd png ; then
		eerror "CGI support requested, bug GD not built with PNG support"
		eerror "Please rebuild gd with 'USE=png'"
		die
	fi
	enewgroup nut 84
	enewuser nut 84 -1 /var/lib/nut nut,uucp
	# As of udev-104, NUT must be in uucp and NOT in tty.
	gpasswd -d nut tty 2>/dev/null
	gpasswd -a nut uucp 2>/dev/null
	use hal && gpasswd -a haldaemon nut 2>/dev/null
	# in some cases on old systems it wasn't in the nut group either!
	gpasswd -a nut nut 2>/dev/null
	warningmsg ewarn
}

src_unpack() {
	unpack ${A}

	cd "${S}"

	ht_fix_file configure.in

	epatch "${FILESDIR}"/${P}-no-libdummy.patch

	sed -e "s:GD_LIBS.*=.*-L/usr/X11R6/lib \(.*\) -lXpm -lX11:GD_LIBS=\"\1:" \
		-i configure.in || die "sed failed"

	sed -e "s:52_nut-usbups.rules:70-nut-usbups.rules:" \
		-i scripts/udev/Makefile.am || die "sed failed"

	WANT_AUTOCONF=2.5 eautoreconf || die "autoconf failed"
}

src_compile() {
	local myconf

	if [ -n "${NUT_DRIVERS}" ]; then
		myconf="${myconf} --with-drivers=${NUT_DRIVERS// /,}"
	fi

	econf \
		--with-user=nut \
		--with-group=nut \
		--with-drvpath=/lib/nut \
		--sysconfdir=/etc/nut \
		--with-logfacility=LOG_DAEMON \
		--with-statepath=/var/lib/nut \
		--with-htmlpath=/usr/share/nut/html \
		--datarootdir=/usr/share/nut \
		--datadir=/usr/share/nut \
		--with-dev \
		$(use_with xml neonxml) \
		$(use_with hal) \
		$(use_with ssl) \
		$(use_with usb) \
		$(use_with snmp) \
		$(use_with cgi) \
		$(use_with cgi cgipath /usr/share/nut/cgi) \
		${myconf} || die "econf failed"

	emake || die "compile problem"

}

src_install() {

	make DESTDIR="${D}" install || die "make install failed"

	dodir /sbin
	dosym /lib/nut/upsdrvctl /sbin/upsdrvctl
	# This needs to exist for the scripts
	dosym /lib/nut/upsdrvctl /usr/sbin/upsdrvctl

	if use cgi; then
		elog "CGI monitoring scripts are installed in /usr/share/nut/cgi."
		elog "copy them to your web server's ScriptPath to activate (this is a"
		elog "change from the old location)."
		elog "If you use lighttpd, see lighttpd_nut.conf in the documentation."
	fi

	# this must be done after all of the install phases
	for i in "${D}"/etc/nut/*.sample ; do
		mv "${i}" "${i/.sample/}"
	done

	dodoc ChangeLog INSTALL MAINTAINERS NEWS README UPGRADING \
			docs/{FAQ,*.txt}

	newdoc lib/README README.lib

	newdoc "${FILESDIR}"/lighttpd_nut.conf-2.2.0 lighttpd_nut.conf

	docinto cables
	dodoc docs/cables/*

	newinitd "${FILESDIR}"/nut-2.2.2-init.d-upsd upsd
	newinitd "${FILESDIR}"/nut-2.2.2-init.d-upsdrv upsdrv
	newinitd "${FILESDIR}"/nut-2.2.2-init.d-upsmon upsmon

	# This sets up permissions for nut to access a UPS
	insinto /etc/udev/rules.d/
	newins scripts/udev/nut-usbups.rules 70-nut-usbups.rules

	keepdir /var/lib/nut

	fperms 0700 /var/lib/nut
	fowners nut:nut /var/lib/nut

	# Do not remove eval here, because the variables contain shell expansions.
	eval fperms 0640 ${NUT_PRIVATE_FILES}
	eval fowners root:nut ${NUT_PRIVATE_FILES}

	# Do not remove eval here, because the variables contain shell expansions.
	eval fperms 0644 ${NUT_PUBLIC_FILES}
	eval fowners root:root ${NUT_PUBLIC_FILES}

	# Do not remove eval here, because the variables contain shell expansions.
	if use cgi; then
		eval fperms 0644 ${NUT_CGI_FILES}
		eval fowners root:root ${NUT_CGI_FILES}
	fi

	# this is installed for 2.4 and fbsd guys
	if ! has_version sys-fs/udev; then
		insinto /etc/hotplug/usb
		insopts -m 755
		doins scripts/hotplug/nut-usbups.hotplug
	fi

	if use hal; then
		insinto /usr/share/hal/fdi/information/20thirdparty/
		doins scripts/hal/20-ups-nut-device.fdi
		insinto /usr/libexec
		insopts -m 755
		doins drivers/hald-addon-*
	    rm "${D}"/lib/nut/hald-addon-*
	fi
}

pkg_postinst() {
	# this is to ensure that everybody that installed old versions still has
	# correct permissions

	chown nut:nut "${ROOT}"/var/lib/nut 2>/dev/null
	chmod 0700 "${ROOT}"/var/lib/nut 2>/dev/null

	# Do not remove eval here, because the variables contain shell expansions.
	eval chown root:nut "${ROOT}"${NUT_PRIVATE_FILES} 2>/dev/null
	eval chmod 0640 "${ROOT}"${NUT_PRIVATE_FILES} 2>/dev/null

	# Do not remove eval here, because the variables contain shell expansions.
	eval chown root:root "${ROOT}"${NUT_PUBLIC_FILES} 2>/dev/null
	eval chmod 0644 "${ROOT}"${NUT_PUBLIC_FILES} 2>/dev/null

	# Do not remove eval here, because the variables contain shell expansions.
	if use cgi; then
		eval chown root:root "${ROOT}"${NUT_CGI_FILES} 2>/dev/null
		eval chmod 0644 "${ROOT}"${NUT_CGI_FILES} 2>/dev/null
	fi

	warningmsg elog
}

warningmsg() {
	msgfunc="$1"
	[ -z "$msgfunc" ] && die "msgfunc not specified in call to warningmsg!"
	${msgfunc} "Please note that NUT now runs under the 'nut' user."
	${msgfunc} "NUT is in the uucp group for access to RS-232 UPS."
	${msgfunc} "However if you use a USB UPS you may need to look at the udev or"
	${msgfunc} "hotplug rules that are installed, and alter them suitably."
	echo
	${msgfunc} "If you use hald, you may be able to skip the normal init scripts."
	echo
	${msgfunc} "You are strongly advised to read the UPGRADING file provided by upstream."
	echo
	${msgfunc} "Please note that upsdrv is NOT automatically started by upsd anymore."
	${msgfunc} "If you have multiple UPS units, you can use their NUT names to"
	${msgfunc} "have a service per UPS:"
	${msgfunc} "ln -s /etc/init.d/upsdrv /etc/init.d/upsdrv.\$UPSNAME"
}
