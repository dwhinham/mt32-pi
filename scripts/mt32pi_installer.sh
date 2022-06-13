#!/usr/bin/env bash

# mt32pi_installer.sh
#
# mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
# Copyright (C) 2020-2022 Dale Whinham <daleyo@gmail.com>
#
# This file is part of mt32-pi.
#
# mt32-pi is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# mt32-pi is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# mt32-pi. If not, see <http://www.gnu.org/licenses/>.

set -o pipefail

SCRIPT_VERSION=0.1.1
GITHUB_API_URL="https://api.github.com/repos/dwhinham/mt32-pi/releases/latest"
MISTER_WPA_SUPPLICANT_CFG_PATH="/media/fat/linux/wpa_supplicant.conf"

DIALOG=(dialog --no-collapse --colors --backtitle "mt32-pi Installer v$SCRIPT_VERSION")
CURL=(curl)

# Ugh... kludge for the broken CA certificate situation on MiSTer
if [ -f /etc/ssl/certs/cacert.pem ]; then
	CURL=(curl -f --cacert /etc/ssl/certs/cacert.pem)
fi

# ISO 3166-1 alpha-2 names and codes from: https://www.iso.org/obp/ui/#search
COUNTRY_CODES=(
	"AD" "Andorra"
	"AE" "United Arab Emirates (the)"
	"AF" "Afghanistan"
	"AG" "Antigua and Barbuda"
	"AI" "Anguilla"
	"AL" "Albania"
	"AM" "Armenia"
	"AO" "Angola"
	"AQ" "Antarctica"
	"AR" "Argentina"
	"AS" "American Samoa"
	"AT" "Austria"
	"AU" "Australia"
	"AW" "Aruba"
	"AX" "Åland Islands"
	"AZ" "Azerbaijan"
	"BA" "Bosnia and Herzegovina"
	"BB" "Barbados"
	"BD" "Bangladesh"
	"BE" "Belgium"
	"BF" "Burkina Faso"
	"BG" "Bulgaria"
	"BH" "Bahrain"
	"BI" "Burundi"
	"BJ" "Benin"
	"BL" "Saint Barthélemy"
	"BM" "Bermuda"
	"BN" "Brunei Darussalam"
	"BO" "Bolivia (Plurinational State of)"
	"BQ" "Bonaire, Sint Eustatius and Saba"
	"BR" "Brazil"
	"BS" "Bahamas (the)"
	"BT" "Bhutan"
	"BV" "Bouvet Island"
	"BW" "Botswana"
	"BY" "Belarus"
	"BZ" "Belize"
	"CA" "Canada"
	"CC" "Cocos (Keeling) Islands (the)"
	"CD" "Congo (the Democratic Republic of the)"
	"CF" "Central African Republic (the)"
	"CG" "Congo (the)"
	"CH" "Switzerland"
	"CI" "Côte d'Ivoire"
	"CK" "Cook Islands (the)"
	"CL" "Chile"
	"CM" "Cameroon"
	"CN" "China"
	"CO" "Colombia"
	"CR" "Costa Rica"
	"CU" "Cuba"
	"CV" "Cabo Verde"
	"CW" "Curaçao"
	"CX" "Christmas Island"
	"CY" "Cyprus"
	"CZ" "Czechia"
	"DE" "Germany"
	"DJ" "Djibouti"
	"DK" "Denmark"
	"DM" "Dominica"
	"DO" "Dominican Republic (the)"
	"DZ" "Algeria"
	"EC" "Ecuador"
	"EE" "Estonia"
	"EG" "Egypt"
	"EH" "Western Sahara"
	"ER" "Eritrea"
	"ES" "Spain"
	"ET" "Ethiopia"
	"FI" "Finland"
	"FJ" "Fiji"
	"FK" "Falkland Islands (the)"
	"FM" "Micronesia (Federated States of)"
	"FO" "Faroe Islands (the)"
	"FR" "France"
	"GA" "Gabon"
	"GB" "United Kingdom of Great Britain and Northern Ireland (the)"
	"GD" "Grenada"
	"GE" "Georgia"
	"GF" "French Guiana"
	"GG" "Guernsey"
	"GH" "Ghana"
	"GI" "Gibraltar"
	"GL" "Greenland"
	"GM" "Gambia (the)"
	"GN" "Guinea"
	"GP" "Guadeloupe"
	"GQ" "Equatorial Guinea"
	"GR" "Greece"
	"GS" "South Georgia and the South Sandwich Islands"
	"GT" "Guatemala"
	"GU" "Guam"
	"GW" "Guinea-Bissau"
	"GY" "Guyana"
	"HK" "Hong Kong"
	"HM" "Heard Island and McDonald Islands"
	"HN" "Honduras"
	"HR" "Croatia"
	"HT" "Haiti"
	"HU" "Hungary"
	"ID" "Indonesia"
	"IE" "Ireland"
	"IL" "Israel"
	"IM" "Isle of Man"
	"IN" "India"
	"IO" "British Indian Ocean Territory (the)"
	"IQ" "Iraq"
	"IR" "Iran (Islamic Republic of)"
	"IS" "Iceland"
	"IT" "Italy"
	"JE" "Jersey"
	"JM" "Jamaica"
	"JO" "Jordan"
	"JP" "Japan"
	"KE" "Kenya"
	"KG" "Kyrgyzstan"
	"KH" "Cambodia"
	"KI" "Kiribati"
	"KM" "Comoros (the)"
	"KN" "Saint Kitts and Nevis"
	"KP" "Korea (the Democratic People's Republic of)"
	"KR" "Korea (the Republic of)"
	"KW" "Kuwait"
	"KY" "Cayman Islands (the)"
	"KZ" "Kazakhstan"
	"LA" "Lao People's Democratic Republic (the)"
	"LB" "Lebanon"
	"LC" "Saint Lucia"
	"LI" "Liechtenstein"
	"LK" "Sri Lanka"
	"LR" "Liberia"
	"LS" "Lesotho"
	"LT" "Lithuania"
	"LU" "Luxembourg"
	"LV" "Latvia"
	"LY" "Libya"
	"MA" "Morocco"
	"MC" "Monaco"
	"MD" "Moldova (the Republic of)"
	"ME" "Montenegro"
	"MF" "Saint Martin (French part)"
	"MG" "Madagascar"
	"MH" "Marshall Islands"
	"MK" "North Macedonia"
	"ML" "Mali"
	"MM" "Myanmar"
	"MN" "Mongolia"
	"MO" "Macao"
	"MP" "Northern Mariana Islands (the)"
	"MQ" "Martinique"
	"MR" "Mauritania"
	"MS" "Montserrat"
	"MT" "Malta"
	"MU" "Mauritius"
	"MV" "Maldives"
	"MW" "Malawi"
	"MX" "Mexico"
	"MY" "Malaysia"
	"MZ" "Mozambique"
	"NA" "Namibia"
	"NC" "New Caledonia"
	"NE" "Niger (the)"
	"NF" "Norfolk Island"
	"NG" "Nigeria"
	"NI" "Nicaragua"
	"NL" "Netherlands (the)"
	"NO" "Norway"
	"NP" "Nepal"
	"NR" "Nauru"
	"NU" "Niue"
	"NZ" "New Zealand"
	"OM" "Oman"
	"PA" "Panama"
	"PE" "Peru"
	"PF" "French Polynesia"
	"PG" "Papua New Guinea"
	"PH" "Philippines (the)"
	"PK" "Pakistan"
	"PL" "Poland"
	"PM" "Saint Pierre and Miquelon"
	"PN" "Pitcairn"
	"PR" "Puerto Rico"
	"PS" "Palestine, State of"
	"PT" "Portugal"
	"PW" "Palau"
	"PY" "Paraguay"
	"QA" "Qatar"
	"RE" "Réunion"
	"RO" "Romania"
	"RS" "Serbia"
	"RU" "Russian Federation (the)"
	"RW" "Rwanda"
	"SA" "Saudi Arabia"
	"SB" "Solomon Islands"
	"SC" "Seychelles"
	"SD" "Sudan (the)"
	"SE" "Sweden"
	"SG" "Singapore"
	"SH" "Saint Helena, Ascension and Tristan da Cunha"
	"SI" "Slovenia"
	"SJ" "Svalbard and Jan Mayen"
	"SK" "Slovakia"
	"SL" "Sierra Leone"
	"SM" "San Marino"
	"SN" "Senegal"
	"SO" "Somalia"
	"SR" "Suriname"
	"SS" "South Sudan"
	"ST" "Sao Tome and Principe"
	"SV" "El Salvador"
	"SX" "Sint Maarten (Dutch part)"
	"SY" "Syrian Arab Republic (the)"
	"SZ" "Eswatini"
	"TC" "Turks and Caicos Islands (the)"
	"TD" "Chad"
	"TF" "French Southern Territories (the)"
	"TG" "Togo"
	"TH" "Thailand"
	"TJ" "Tajikistan"
	"TK" "Tokelau"
	"TL" "Timor-Leste"
	"TM" "Turkmenistan"
	"TN" "Tunisia"
	"TO" "Tonga"
	"TR" "Turkey"
	"TT" "Trinidad and Tobago"
	"TV" "Tuvalu"
	"TW" "Taiwan (Province of China)"
	"TZ" "Tanzania, the United Republic of"
	"UA" "Ukraine"
	"UG" "Uganda"
	"UM" "United States Minor Outlying Islands (the)"
	"US" "United States of America (the)"
	"UY" "Uruguay"
	"UZ" "Uzbekistan"
	"VA" "Holy See (the)"
	"VC" "Saint Vincent and the Grenadines"
	"VE" "Venezuela (Bolivarian Republic of)"
	"VG" "Virgin Islands (British)"
	"VI" "Virgin Islands (U.S.)"
	"VN" "Viet Nam"
	"VU" "Vanuatu"
	"WF" "Wallis and Futuna"
	"WS" "Samoa"
	"YE" "Yemen"
	"YT" "Mayotte"
	"ZA" "South Africa"
	"ZM" "Zambia"
	"ZW" "Zimbabwe"
)

# awk script for editing options in a .ini-style config file
read -r -d '' AWK_SCRIPT <<'EOF'
BEGIN {
	in_section = 0;
}

$0 ~ "^\\[" section "\\]$" {
	in_section = 1;
}

$0 ~ "^" key " = .+$" {
	if (in_section) {
		print $1 " = " value;
		skip = 1;
	}
}

/^\[.+\]\$/ {
	in_section = 0;
}

/.*/ {
	if (skip)
		skip = 0;
	else
		print $0;
}
EOF

IFS=$'\n'
read -r -d '' MT32PI_LOGO <<'EOF'
 \Z5                                 ________    _______                      __
                         __      /_____   `. /____   `.                   /__`
     ____   ____     __/  /____  _______)  / ______)  / ____  ______      __
   /   __ v  __  `./__   _____//_____    < /   _____. /____//   ___  `. /  /
  /  /  /  /  /  /   /  /____  _______)  //  /______       /  /____/  //  .__
 /__/  /__/  /__/    \______//_________. /_________/      /   ______.  \____/
 \Z2/////////////////////////////////////////////////////// \Z5/  / \Z2//// /// // /
                                                         \Z5```\Zn
EOF
unset IFS

function die {
	clear
	if [ "$@" ]; then
		echo "$@" >&2
	fi
	exit 1
}

function check_tool {
	if ! command -v "$1" &>/dev/null; then
		die "Command '$1' is unavailable. Please install it using your system package manager."
	fi
}

function get_latest_version {
	local temp_dir temp_file release_url

	temp_dir="$1"
	temp_file=$(mktemp) || return 1
	release_url=$("${CURL[@]}" -s -L $GITHUB_API_URL | jq -r ".assets[0].browser_download_url") || return 1

	"${CURL[@]}" -L -o "$temp_file" "$release_url" 2>&1 | "${DIALOG[@]}" --progressbox "Downloading mt32-pi..." 30 83 || return 1
	unzip "$temp_file" -d "$temp_dir" | "${DIALOG[@]}" --progressbox "Extracting mt32-pi..." 30 83 || return 1
	rm -f "$temp_file"
}

function set_ini_option {
	local tmp_file
	tmp_file=$(mktemp)
	awk "$AWK_SCRIPT" section="$2" key="$3" value="$4" "$1" >"$tmp_file"
	mv "$tmp_file" "$1"
}

function list_disks {
	local name dev size

	# Search for USB-connected disks or disks which have the MMC_TYPE property that have a size >0 (are inserted)
	for dev in /sys/block/*; do
		if udevadm info --query=property --path="$dev" | grep -q -e ^ID_BUS=usb -e ^MMC_TYPE && [ "$(cat "$dev"/size)" -gt 0 ]; then
			name="$(cat "$dev"/device/vendor 2>/dev/null) $(cat "$dev"/device/model 2>/dev/null)"
			dev=/dev/$(basename "$dev")
			size=$(blockdev --getsize64 "$dev" | numfmt --to=iec-i --format=%.2f --suffix=B)
			echo "$dev"
			echo "$name ($size)"
		fi
	done
}

function prepare_disk {
	local disk
	local mount_point
	local partition

	disk="$1"
	mount_point="$2"
	if [[ "$disk" =~ ^.+[0-9]+$ ]]; then
		partition="${disk}p1"
	else
		partition="${disk}1"
	fi

	echo 0 | "${DIALOG[@]}" --gauge "Unmounting partitions on \Zb\Z4${disk}\Zn..." 6 70
	umount "$disk"?* >/dev/null 2>&1
	echo 25 | "${DIALOG[@]}" --gauge "Creating partition table..." 6 70
	parted --script "$disk" mklabel msdos mkpart primary fat32 0% 100% || return 1
	echo 50 | "${DIALOG[@]}" --gauge "Formatting \Zb\Z4${partition}\Zn as FAT32..." 6 70
	mkfs.fat -F 32 -n MT32-PI "${partition}" >/dev/null || return 1
	echo 75 | "${DIALOG[@]}" --gauge "Mounting \Zb\Z4${partition}\Zn..." 6 70
	mount "${partition}" "$mount_point" || return 1
	echo 100 | "${DIALOG[@]}" --gauge "Complete!" 6 70
}

function get_wifi_country_code {
	"${DIALOG[@]}" --title "Network configuration" --menu "Please choose your country (to ensure the Wi-Fi radio complies with regulations and improve compatibility):" 30 80 10 "${COUNTRY_CODES[@]}" --output-fd 1
}

if [ "$EUID" -ne 0 ]; then
	echo "This script must be run as root."
	echo "Please try 'sudo $0'."
	exit 1
fi

# Ensure all required tools are available
check_tool awk
check_tool curl
check_tool dialog
check_tool grep
check_tool jq
check_tool mkfs.fat
check_tool parted
check_tool sed
check_tool udevadm
check_tool unzip

read -r -d '' MSG_WELCOME <<EOF
$MT32PI_LOGO
This tool will help you prepare an SD card for your Raspberry Pi, and install the latest version of mt32-pi onto it.

Please ensure you have inserted an SD card into a USB card reader and connected it to your machine, then press Enter to continue.

\Z1\ZuNOTE:\Zn
It is \Z1\ZuSTRONGLY RECOMMENDED\Zn that you unplug any other USB storage devices to prevent accidental data loss!

\Z1\ZuNOTE 2:\Zn
This tool is \Z1\ZuEXPERIMENTAL\Zn! You get to keep the pieces if it breaks!
EOF

"${DIALOG[@]}" --title "Welcome to the mt32-pi Installer!" --yes-label "Proceed" --no-label "Quit" --yesno "$MSG_WELCOME" 24 82 || die "Installation aborted."

disks=$(list_disks)

if [ -z "$disks" ]; then
	"${DIALOG[@]}" --title "Error" --msgbox "No SD cards were found. Installation can not continue." 5 70
	die "Installation aborted."
fi

mapfile -t disks < <(list_disks)
disk=$("${DIALOG[@]}" --title "SD card selection" --menu "Please select your SD card from the list:" 10 70 10 "${disks[@]}" --output-fd 1) || die "Installation aborted."

read -r -d '' MSG_DISK_CONFIRMATION <<EOF
You chose \Zb\Z4$disk\Zn.

We will now:

  \Z1*\Zn Unmount any mounted partitions from this disk.
  \Z1*\Zn Erase the partition table.
  \Z1*\Zn Format a new FAT32 partition spanning the whole disk.

Are you sure you want to continue?
EOF

read -r -d '' MSG_DISK_CONFIRMATION_2 <<EOF
\Z1\ZuWARNING:\Zn

About to erase the contents of \Zb\Z4$disk\Zn.

        >>> \Z1\ZuTHIS IS YOUR LAST CHANCE TO ABORT\Zn <<<

           Are you sure you want to continue?
EOF

"${DIALOG[@]}" --title "SD card partitioning and formatting" --yesno "$MSG_DISK_CONFIRMATION" 13 65 \
	       --colors --no-collapse --yesno "$MSG_DISK_CONFIRMATION_2" 11 60 || die "Installation aborted."

mount_point=$(mktemp -d) || die "Couldn't create temporary directory"
prepare_disk "$disk" "$mount_point" || die "Unable to partition and format disk"
get_latest_version "$mount_point" || die "Couldn't download latest version"
mt32_pi_config="$mount_point/mt32-pi.cfg"
wpa_supplicant="$mount_point/wpa_supplicant.conf"
config_txt="$mount_point/config.txt"

set_ini_option "$mt32_pi_config" audio output_device i2s
set_ini_option "$mt32_pi_config" control scheme simple_encoder
set_ini_option "$mt32_pi_config" control mister on
set_ini_option "$mt32_pi_config" lcd type ssd1306_i2c
set_ini_option "$mt32_pi_config" lcd height 64

read -r -d '' MSG_NETWORK_CONFIG <<EOF
Would you like to enable Wi-Fi and the embedded FTP server on mt32-pi?

This will allow you to remotely access the filesystem and also make use of update scripts.

You will be asked to type your Wi-Fi access point name and password.
EOF

read -r -d '' MSG_WPA_SUPPLICANT_CFG_DETECTED <<EOF
A Wi-Fi configuration file was detected at \Zb\Z4$MISTER_WPA_SUPPLICANT_CFG_PATH\Zn.

Would you like to use these settings for mt32-pi, too?
EOF

read -r -d '' MSG_WPA_SUPPLICANT_CFG_NO_COUNTRY <<EOF
The MiSTer Wi-Fi settings file does not contain a country code, so it probably won't work on mt32-pi as-is.

Please choose your country on the next screen. The country code will then be added to mt32-pi's Wi-Fi configuration.
EOF

if "${DIALOG[@]}" --title "Network configuration" --yesno "$MSG_NETWORK_CONFIG" 10 80; then
	set_ini_option "$mt32_pi_config" network mode wifi
	set_ini_option "$mt32_pi_config" network ftp on

	if [ -f "$MISTER_WPA_SUPPLICANT_CFG_PATH" ] && \
	   "${DIALOG[@]}" --title "Network configuration" --yesno "$MSG_WPA_SUPPLICANT_CFG_DETECTED" 7 90; then
		cp "$MISTER_WPA_SUPPLICANT_CFG_PATH" "$wpa_supplicant"
		if ! grep -qE "country\s*=\s*[A-Z]{2}" "$wpa_supplicant"; then
			"${DIALOG[@]}" --title "Network configuration" --msgbox "$MSG_WPA_SUPPLICANT_CFG_NO_COUNTRY" 9 90
			wifi_country_code=$(get_wifi_country_code)
			sed -i "1s/^/country=$wifi_country_code\n\n/" "$wpa_supplicant"
		fi
		"${DIALOG[@]}" --title "Network configuration" --msgbox "Your Wi-Fi settings have been copied.\n\nWi-Fi and the embedded FTP server have been enabled." 7 60
	else
		wifi_country_code=$(get_wifi_country_code)
		if [ "$wifi_country_code" ]; then
			wifi_ssid=$("${DIALOG[@]}" --title "Network configuration" --inputbox "Please enter your Wi-Fi access point name:" 8 50 --output-fd 1)
			if [ "$wifi_ssid" ]; then
				wifi_password=$("${DIALOG[@]}" --title "Network configuration" --insecure --passwordbox "Please enter your Wi-Fi password:" 8 50 --output-fd 1)
				if [ "$wifi_password" ]; then
					sed -i "s/country=GB/country=$wifi_country_code/g" "$wpa_supplicant"
					sed -i "s/my-ssid/$wifi_ssid/g" "$wpa_supplicant"
					sed -i "s/my-password/$wifi_password/g" "$wpa_supplicant"

					"${DIALOG[@]}" --title "Network configuration" --msgbox "Wi-Fi and the embedded FTP server have been enabled." 5 60
				fi
			fi
		fi
	fi
fi

read -r -d '' MSG_LOW_VOLTAGE <<EOF
Would you like to disable low voltage warnings?

You will usually need this when powering your mt32-pi from the MiSTer's user port due to voltage drop.

This is normal.

\Z1\ZuNOTE:\Zn
Please make sure you are using a heatsink or a case incorporating some sort of cooling for the Raspberry Pi.
EOF

if "${DIALOG[@]}" --title "Low voltage warnings" --yesno "$MSG_LOW_VOLTAGE" 14 60; then
	"${DIALOG[@]}" --title "Low voltage warnings" --msgbox "Low voltage warnings have been disabled." 5 45
	sed -i 's/#avoid_warnings=2/avoid_warnings=2/g' "$config_txt"
fi

(echo "Syncing changes to disk and unmounting, please wait..."; umount "$mount_point") | "${DIALOG[@]}" --progressbox 3 60 || die "Failed to unmount SD card"
rm -rf "$mount_point"

read -r -d '' MSG_COMPLETE <<EOF
It's now safe to remove the SD card from your card reader and insert it into your Raspberry Pi.

\Z1\ZuNOTE:\Zn
MT-32 mode will be unavailable until you add MT-32 ROM files to the \Zb\Z4roms\Zn directory on the SD card.

             Thankyou for using mt32-pi! \Z5<3\Zn

           \Z1\Zuhttps://github.com/dwhinham/mt32-pi\Zn

         Please support open source developers!
EOF

"${DIALOG[@]}" --title "Installation complete!" --msgbox "$MSG_COMPLETE" 16 60

clear
