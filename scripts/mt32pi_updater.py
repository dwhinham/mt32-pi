#!/usr/bin/env python3

# mt32pi_updater.py
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

from configparser import ConfigParser
from datetime import datetime
from ftplib import FTP, error_temp
try:
	from packaging.version import parse as parse_version
except ImportError:
	# Fall back on pkg_resources when packaging is unavailable
	from pkg_resources import parse_version
from pathlib import Path
from sys import stderr
from time import sleep
from urllib import request

import json
import os
import re
import shutil
import socket
import tempfile

# -----------------------------------------------------------------------------
# User options
# -----------------------------------------------------------------------------

# Your mt32-pi's hostname or IP address
MT32PI_FTP_HOST = "mt32-pi"

# The FTP username/password as specified in mt32-pi.cfg
MT32PI_FTP_USERNAME = "mt32-pi"
MT32PI_FTP_PASSWORD = "mt32-pi"

# List of paths within the update package to skip; trailing slash indicates
# entire directory
NO_UPDATE = [
	"roms/",
	"soundfonts/",
]

# Set this to True to skip the version check and update anyway
FORCE_UPDATE = False

# Set this to False to skip showing the release notes
SHOW_RELEASE_NOTES = True

# Try increasing this value if your network isn't very reliable
CONNECTION_TIMEOUT_SECS = 5

# -----------------------------------------------------------------------------
# End of user options
# -----------------------------------------------------------------------------

GITHUB_API_URL = "https://api.github.com/repos/dwhinham/mt32-pi/releases"
SCRIPT_VERSION = "0.1.0"

CLEAR_TERM = "\033c"
COLOR_RED = "\033[31;1m"
COLOR_GREEN = "\033[32;1m"
COLOR_YELLOW = "\033[33;1m"
COLOR_PURPLE = "\033[35;1m"
COLOR_BRIGHT_WHITE = "\033[97;1m"
COLOR_RESET = "\033[0m"

MT32PI_LOGO = """\
  {}{}
                                   ________    _______                      __
                          __      /_____   `. /____   `.                   /__`
      ____   ____     __/  /____  _______)  / ______)  / ____  ______      __
    /   __ v  __  `./__   _____//_____    < /   _____. /____//   ___  `. /  /
   /  /  /  /  /  /   /  /____  _______)  //  /______       /  /____/  //  .__
  /__/  /__/  /__/    \______//_________. /_________/      /   ______.  \____/
  {}/////////////////////////////////////////////////////// {}/  / {}//// /// // /
                                                          {}```{}
  {:>78}
""".format(
	"Welcome to the",
	COLOR_GREEN, COLOR_PURPLE, COLOR_GREEN, COLOR_PURPLE, COLOR_GREEN, COLOR_RESET,
	f"update script v{SCRIPT_VERSION}, © Dale Whinham 2020-2022"
)

OLD_FILE_NAMES = [
	'mt32-pi.cfg',          # mt32-pi main configuration file
	'config.txt',           # Raspberry Pi configuration file
	'wpa_supplicant.conf',  # Wi-Fi configuration file
]

RESULT_COLUMN_WIDTH = 10
ANSI_ESCAPE_REGEX = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')

def print_status(message):
	stripped_message = ANSI_ESCAPE_REGEX.sub('', message)
	length_diff = len(message) - len(stripped_message)

	term_width = shutil.get_terminal_size()[0] + length_diff
	print(f"{message:{term_width}}", end="", flush=True)

def print_result(message, color=None, replace=False):
	clear_width = RESULT_COLUMN_WIDTH
	print("\b" * clear_width, end="")

	if color:
		print(color, end="")

	print(f"{message:>{clear_width + 1}}", end="" if replace else "\n", flush=True)

	if color:
		print(COLOR_RESET, end="", flush=True)

# -----------------------------------------------------------------------------
# Download/upload functions
# -----------------------------------------------------------------------------
def get_current_version():
	print_status(f"Connecting to your mt32-pi's embedded FTP server at '{COLOR_PURPLE}{MT32PI_FTP_HOST}{COLOR_RESET}'...")

	try:
		with FTP(MT32PI_FTP_HOST, MT32PI_FTP_USERNAME, MT32PI_FTP_PASSWORD, timeout=CONNECTION_TIMEOUT_SECS) as ftp:
			print_result("OK!", COLOR_GREEN)
			result = re.search(r"mt32-pi (v[0-9]+.[0-9]+.[0-9]+)", ftp.getwelcome())
			return result.group(1)

	except socket.timeout:
		print_result("FAILED!", COLOR_RED)
		print(f"Couldn't connect to your mt32-pi - you did enable networking and the FTP server in {COLOR_PURPLE}mt32-pi.cfg{COLOR_RESET}, right?")
		return None

	except:
		print_result("FAILED!", COLOR_RED)
		print("Failed to extract version number from FTP welcome message.")
		return None

def get_old_data(temp_dir):
	print_status(f"Connecting to your mt32-pi's embedded FTP server at '{COLOR_PURPLE}{MT32PI_FTP_HOST}{COLOR_RESET}'...")

	try:
		with FTP(MT32PI_FTP_HOST, MT32PI_FTP_USERNAME, MT32PI_FTP_PASSWORD, timeout=CONNECTION_TIMEOUT_SECS) as ftp:
			print_result("OK!", COLOR_GREEN)

			for file_name in OLD_FILE_NAMES:
				with open(temp_dir / file_name, 'wb') as file:
					# TODO: Handle when file doesn't exist
					print_status(f"Retrieving {COLOR_PURPLE}{file_name}{COLOR_RESET}...")
					try:
						ftp.retrbinary(f"RETR /SD/{file_name}", file.write)
						print_result("DONE!", COLOR_GREEN)
					except error_temp:
						print_result("NOT FOUND!", COLOR_YELLOW)

		return True

	except socket.timeout:
		print_result("FAILED!", COLOR_RED)
		print(f"Couldn't connect to your mt32-pi - you did enable networking and the FTP server in {COLOR_PURPLE}mt32-pi.cfg{COLOR_RESET}, right?")
		return False

def get_latest_release_info():
	releases = []
	try:
		print_status("Retrieving release info from GitHub...")
		with request.urlopen(GITHUB_API_URL) as response:
			charset = response.info().get_param('charset') or 'utf-8'
			releases = json.loads(response.read().decode(charset))
		print_result("OK!", COLOR_GREEN)
	except:
		print_result("FAILED!", COLOR_RED)
		print("Failed to retrieve release info from GitHub.", file=stderr)
		return None

	# Sort by version number in descending order
	releases.sort(reverse=True, key=lambda release: parse_version(release['tag_name']))
	return releases[0]


def download_and_extract_release(release_info, destination_path):
	asset = release_info['assets'][0]
	url = asset['browser_download_url']
	file_name = asset['name']
	path = Path(destination_path) / file_name

	print_status(f"Downloading {COLOR_PURPLE}{file_name}{COLOR_RESET}...")
	with request.urlopen(url) as response, open(path, 'wb') as out_file:
		shutil.copyfileobj(response, out_file)
		print_result("OK!", COLOR_GREEN)

	print_status(f"Unpacking {COLOR_PURPLE}{file_name}{COLOR_RESET}...")
	shutil.unpack_archive(path, Path(destination_path) / "install")
	print_result("OK!", COLOR_GREEN)

# -----------------------------------------------------------------------------
# Config file processing functions
# -----------------------------------------------------------------------------
def find_section(lines, section):
	for index, line in enumerate(lines):
		if line.strip() == f"[{section}]":
			return index
	return None

def is_a_section(line):
	return re.match(r"^\[.+\]$", line.strip()) != None

def append_new_section(config_lines, section):
	index = len(config_lines)
	if index > 0:
		config_lines.append("")
		index += 1
	config_lines.append(f"[{section}]")
	return index

def find_option(config_lines, start_index, key):
	if start_index < len(config_lines):
		for i in range(start_index, len(config_lines)):
			line = config_lines[i]
			if re.match(rf"^{key}\s*=\s*.+$", line.strip()):
				return i
			if is_a_section(line):
				break
	return None

def insert_new_option_line(config_lines, start_index, key, value):
	option_line = f"{key} = {value}"

	# Section header is at the end of the file
	if start_index >= len(config_lines):
		config_lines.append(option_line)
		return

	# Find last line of the section that isn't empty
	option_index = start_index
	while option_index + 1 < len(config_lines) and not is_a_section(config_lines[option_index]):
		option_index += 1

	while option_index - 1 > 0 and not config_lines[option_index - 1].strip():
		option_index -= 1

	config_lines.insert(option_index, option_line)


def update_option(config_lines, section, key, value):
	section_index = find_section(config_lines, section)
	if section_index is None:
		section_index = append_new_section(config_lines, section)
	option_index = find_option(config_lines, section_index + 1, key)

	if option_index:
		#print(f"Found {section}/{key} on line {option_index}")
		config_lines[option_index] = f"{key} = {value}"
	else:
		#print(f"Couldn't find {section}/{key}")
		insert_new_option_line(config_lines, section_index + 1, key, value)


def merge_configs(old_config_path, new_config_path):
	print_status("Merging your old settings into the new config file template...")

	try:
		config_old = ConfigParser()
		config_old.read(old_config_path)
		#print({ section: dict(config_old.items(section)) for section in config_old.sections() })

		new_config_lines = []
		with open(install_dir / "mt32-pi.cfg", "r") as in_file:
			new_config_lines = in_file.read().splitlines()

		for section in config_old.sections():
			for item in config_old.items(section):
				key = item[0]
				value = item[1]
				update_option(new_config_lines, section, key, value)

		with open(new_config_path, "w") as out_file:
			config_text = '\n'.join(new_config_lines) + '\n'
			out_file.write(config_text)

		print_result("OK!", COLOR_GREEN)
		return True

	except:
		print_result("FAILED!", COLOR_RED)
		return False

def install(source_path):
	print_status(f"Connecting to your mt32-pi's embedded FTP server at '{COLOR_PURPLE}{MT32PI_FTP_HOST}{COLOR_RESET}'...")

	filter_dirs = [dir[:-1] for dir in NO_UPDATE if dir.endswith("/")]
	filter_files = [file for file in NO_UPDATE if not file.endswith("/")]

	try:
		with FTP(MT32PI_FTP_HOST, MT32PI_FTP_USERNAME, MT32PI_FTP_PASSWORD) as ftp:
			print_result("OK!", COLOR_GREEN)
			#ftp.set_debuglevel(2)

			for (dir_path, dir_names, filenames) in os.walk(source_path):
				remote_dir = Path(dir_path.replace(str(source_path), "").lstrip("/"))

				for file_name in filenames:
					local_file_path = Path(dir_path) / file_name
					remote_file_path = remote_dir / file_name
					print_status(f"Uploading {COLOR_PURPLE}{remote_file_path}{COLOR_RESET}...")

					if str(remote_dir) in filter_dirs or str(remote_file_path) in filter_files:
						print_result("SKIPPED!", COLOR_YELLOW)
						continue

					with open(local_file_path, 'rb') as file:
						file.seek(0, os.SEEK_END)
						file_size = file.tell()
						file.seek(0)

						transferred = 0
						def callback(bytes):
							nonlocal transferred
							transferred += len(bytes)
							print_result(f"[{(transferred / file_size * 100):>6.2f}%]", replace=True)

						ftp.storbinary(f"STOR /SD/{remote_file_path}", file, callback=callback)
						print_result("DONE!", COLOR_GREEN)
		return True

	except socket.timeout:
		print_result("FAILED!", COLOR_RED)
		print(f"Couldn't connect to your mt32-pi - you did enable networking and the FTP server in {COLOR_PURPLE}mt32-pi.cfg{COLOR_RESET}, right?")
		return False

def show_release_notes(release_info):
	text = release_info['body']

	# Poor man's Markdown formatting
	text = re.sub(r"\[(.+?)\]\(.+?\)", rf"{COLOR_PURPLE}\1{COLOR_RESET}", text, flags=re.MULTILINE)
	text = re.sub(r"^#+\s*(.+)$", rf"{COLOR_GREEN}\1{COLOR_RESET}", text, flags=re.MULTILINE)
	text = re.sub(r"^(\s*)-(.+)$", rf"\1{COLOR_PURPLE}-{COLOR_RESET}\2", text, flags=re.MULTILINE)
	text = re.sub(r"^(\s*)\*(.+)$", rf"\1{COLOR_PURPLE}*{COLOR_RESET}\2", text, flags=re.MULTILINE)
	text = re.sub(r"(\*\*|__)(.+)(\*\*|__)", rf"{COLOR_BRIGHT_WHITE}\2{COLOR_RESET}", text, flags=re.MULTILINE)
	text = re.sub(r"`(.+?)`", rf"{COLOR_PURPLE}\1{COLOR_RESET}", text, flags=re.MULTILINE)

	date = datetime.strptime(release_info['published_at'], "%Y-%m-%dT%H:%M:%SZ").strftime("%Y-%m-%d")
	release_header = f"{release_info['tag_name']} - {date}"
	underline = "".join(["=" for i in range(0, len(release_header))])
	print(f"{COLOR_GREEN}{release_header}\n{underline}{COLOR_RESET}\n")

	print(text)

	for i in range(0, 10):
		print(f"\rContinuing in {10 - i}... ", end="", flush=True)
		sleep(1)

	print()

def reboot():
	with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
		sock.sendto(bytes.fromhex("F0 7D 00 F7"), (MT32PI_FTP_HOST, 1999))

# -----------------------------------------------------------------------------
# Entry point
# -----------------------------------------------------------------------------
if __name__ == "__main__":
	print(CLEAR_TERM)
	print(MT32PI_LOGO)

	# Create temporary directory and get the latest release
	with tempfile.TemporaryDirectory(dir=".") as temp_dir_name:
		temp_dir = Path(temp_dir_name)

		current_version = get_current_version() or exit(1)
		release_info = get_latest_release_info() or exit(1)
		latest_version = release_info['tag_name']

		print()
		print(f"The currently-installed version is: {COLOR_GREEN}{current_version}{COLOR_RESET}")
		print(f"The latest release is:              {COLOR_GREEN}{latest_version}{COLOR_RESET}")
		print()

		if not FORCE_UPDATE and parse_version(current_version) >= parse_version(latest_version):
			print("Your mt32-pi is up to date.")
			exit(0)

		if SHOW_RELEASE_NOTES:
			show_release_notes(release_info)

		download_and_extract_release(release_info, temp_dir)

		# Fetch old configs
		get_old_data(temp_dir) or exit(1)

		install_dir = temp_dir / "install"
		old_config_path = install_dir / "mt32-pi.cfg.bak"
		new_config_path = install_dir / "mt32-pi.cfg"

		# Create backup of old config and merge with new
		shutil.move(temp_dir / "mt32-pi.cfg", old_config_path)
		merge_configs(old_config_path, new_config_path) or exit(1)

		# Move the new Wi-Fi/boot configs aside as *.new files in case the user needs to adapt them
		shutil.move(install_dir / "config.txt", install_dir / "config.txt.new")
		shutil.move(temp_dir / "config.txt", install_dir)

		old_wifi_config_path = temp_dir / "wpa_supplicant.conf"
		old_wifi_config_exists = old_wifi_config_path.exists()
		if old_wifi_config_exists:
			shutil.move(install_dir / "wpa_supplicant.conf", install_dir / "wpa_supplicant.conf.new")
			shutil.move(old_wifi_config_path, install_dir)

		# Upload new version
		install(install_dir) or exit(1)

		# Reboot mt32-pi
		reboot()

		print("\nAll done.")
		print(f"The settings from your old config file have been merged into the latest config template.")
		print(f"A backup of your old config is available as {COLOR_PURPLE}mt32-pi.cfg.bak{COLOR_RESET} on the root of your Raspberry Pi's SD card.")
		print(f"Your {COLOR_PURPLE}config.txt{COLOR_RESET} has been preserved.")
		if old_wifi_config_exists:
			print(f"Your {COLOR_PURPLE}wpa_supplicant.conf{COLOR_RESET} has been preserved.")
		print(f"\n{COLOR_GREEN}Your mt32-pi should be automatically rebooting if UDP MIDI is enabled. Otherwise, please power-cycle your mt32-pi.{COLOR_RESET}")
