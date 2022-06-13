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

# -----------------------------------------------------------------------------
# Changelog
# -----------------------------------------------------------------------------
# 0.2.3 - 2022-06-13
# - Remove deprecated options from config file.
#
# 0.2.2 - 2022-04-13
# - Disable colors if colorama or Windows Terminal unavailable on Windows.
# - Fix text alignment and error message colors on Windows.
# - Fix ignore list on Windows.
# - Continue anyway if version number invalid (e.g. test build).
# - Fix self-update file encoding on Windows.
# - Fix screen clearing ANSI code.
#
# 0.2.1 - 2022-04-04
# - Implemented retries for timed-out/failed FTP operations.
#
# 0.2.0 - 2022-03-31
# - User settings moved to external config file (mt32pi_updater.cfg).
# - Script now updates and relaunches itself.
# - Improved error handling.
# - FTP session kept open and re-used between steps.
# - Progress now shown when downloading update package.
# - Colorama used if present for Windows terminal colors.
#
# 0.1.0 - 2022-03-25
# - Initial version.
# -----------------------------------------------------------------------------

import io
import json
import os
import platform
import re
import shutil
import socket
import sys
import tempfile
from configparser import ConfigParser
from datetime import datetime
from ftplib import FTP, error_temp
from pathlib import Path
from time import sleep
from urllib import request

try:
    # ANSI colors for Windows command prompt
    from colorama import init

    init()
except ImportError:
    pass

try:
    from packaging.version import parse as parse_version
except ImportError:
    # Fall back on pkg_resources when packaging is unavailable
    from pkg_resources import parse_version

GITHUB_REPO = "dwhinham/mt32-pi"
GITHUB_API_URL = f"https://api.github.com/repos/{GITHUB_REPO}/releases"
SCRIPT_URL = f"https://github.com/{GITHUB_REPO}/raw/master/scripts/mt32pi_updater.py"
SCRIPT_VERSION = "0.2.3"

# Config keys
K_SECTION = "updater"
K_HOST = "host"
K_FTP_USERNAME = "ftp_username"
K_FTP_PASSWORD = "ftp_password"
K_CONNECTION_TIMEOUT = "connection_timeout"
K_IGNORE_LIST = "ignore_list"
K_SELF_UPDATE = "self_update"
K_FORCE_UPDATE = "force_update"
K_SHOW_RELEASE_NOTES = "show_release_notes"

# Default config values
DEFAULT_CONFIG = {
    K_SECTION: {
        K_HOST: "mt32-pi",
        K_FTP_USERNAME: "mt32-pi",
        K_FTP_PASSWORD: "mt32-pi",
        K_CONNECTION_TIMEOUT: 5,
        K_SELF_UPDATE: True,
        K_FORCE_UPDATE: False,
        K_SHOW_RELEASE_NOTES: True,
        K_IGNORE_LIST: "roms/, soundfonts/",
    }
}

HAVE_ANSI = (
    "colorama" in sys.modules
    or os.environ.get("WT_SESSION")  # Crude way to detect Windows Terminal
    or platform.system() != "Windows"
)

if HAVE_ANSI:
    CLEAR_TERM = "\033[H\033[J"
    COLOR_RED = "\033[31;1m"
    COLOR_GREEN = "\033[32;1m"
    COLOR_YELLOW = "\033[33;1m"
    COLOR_PURPLE = "\033[35;1m"
    COLOR_BRIGHT_WHITE = "\033[97;1m"
    COLOR_RESET = "\033[0m"
else:
    CLEAR_TERM = ""
    COLOR_RED = ""
    COLOR_GREEN = ""
    COLOR_YELLOW = ""
    COLOR_PURPLE = ""
    COLOR_BRIGHT_WHITE = ""
    COLOR_RESET = ""

MT32PI_LOGO = r"""
  {}{}
                                   ________    _______                      __
                          __      /_____   `. /____   `.                   /__`
      ____   ____     __/  /____  _______)  / ______)  / ____  ______      __
    /   __ v  __  `./__   _____//_____    < /   _____. /____//   ___  `. /  /
   /  /  /  /  /  /   /  /____  _______)  //  /______       /  /____/  //  .__
  /__/  /__/  /__/    \______//_________. /_________/      /   ______.  \____/
  {}/////////////////////////////////////////////////////// {}/  / {}//// /// // /
                                                          {}```{}
  {:>76}
""".format(
    "Welcome to the",
    COLOR_GREEN,
    COLOR_PURPLE,
    COLOR_GREEN,
    COLOR_PURPLE,
    COLOR_GREEN,
    COLOR_RESET,
    f"update script v{SCRIPT_VERSION}, © Dale Whinham 2020-2022",
)

RESULT_COLUMN_WIDTH = 10
ANSI_ESCAPE_REGEX = re.compile(r"\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])")

OLD_FILE_NAMES = [
    "mt32-pi.cfg",  # mt32-pi main configuration file
    "config.txt",  # Raspberry Pi configuration file
    "wpa_supplicant.conf",  # Wi-Fi configuration file
]

DEPRECATED_OPTIONS = {
    "audio": [
        "i2c_dac_address",
        "i2c_dac_init",
    ]
}


def print_status(message):
    stripped_message = ANSI_ESCAPE_REGEX.sub("", message)
    length_diff = len(message) - len(stripped_message)

    term_width = shutil.get_terminal_size()[0] + length_diff - 1
    print(f"{message:{term_width}}", end="", flush=True)


def print_result(message, color=None, replace=False):
    clear_width = RESULT_COLUMN_WIDTH
    print("\b" * clear_width, end="")

    if color:
        print(color, end="")

    print(f"{message:>{clear_width}}", end="" if replace else "\n", flush=True)

    if color:
        print(COLOR_RESET, end="", flush=True)


def print_retry(attempt):
    print_result(f"RETRY {attempt}/{RetryFTP.MAX_RETRIES}", COLOR_YELLOW, replace=True)


def print_socket_error():
    print(
        "Couldn't connect to your mt32-pi - you did enable networking and the FTP"
        f" server in {COLOR_PURPLE}mt32-pi.cfg{COLOR_RESET}, right?",
        file=sys.stderr,
    )
    print(
        "This script requires that you are running mt32-pi"
        f" {COLOR_PURPLE}v0.11.0{COLOR_RESET} or above.",
        file=sys.stderr,
    )


def print_socket_timeout():
    print(
        "Connection timed out. Please check your network connection and try again.",
        file=sys.stderr,
    )


def pause(seconds=10):
    clear_length = 0
    for i in range(0, seconds):
        print("\r" + " " * clear_length + "\r", end="")
        countdown = f"Continuing in {seconds - i}..."
        clear_length = len(countdown)
        print(countdown, end="", flush=True)
        sleep(1)

    print("\r" + " " * clear_length + "\r", end="", flush=True)


def restart():
    args = sys.argv[:]
    args.insert(0, sys.executable)
    if sys.platform == "win32":
        args = [f'"{arg}"' for arg in args]

    os.chdir(os.getcwd())
    os.execv(sys.executable, args)


# -----------------------------------------------------------------------------
# Custom ftplib wrapper with retry functionality
# -----------------------------------------------------------------------------
class RetryFTP:
    MAX_RETRIES = 5

    def __init__(self, *args, **kwargs):
        self.ftp_args = args
        self.ftp_kwargs = kwargs
        self.on_retry = None
        self.__reconnect()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.ftp.close()

    def __reconnect(self):
        try:
            self.ftp.quit()
        except Exception:
            pass
        self.ftp = FTP(*self.ftp_args, **self.ftp_kwargs)

    def __retrywrapper(self, func, *args, **kwargs):
        attempt = 0
        retry = False

        while True:
            try:
                if retry:
                    if self.on_retry:
                        self.on_retry(attempt)
                    self.__reconnect()
                    retry = False
                func(self.ftp, *args, **kwargs)
                break

            except (EOFError, error_temp, socket.timeout, socket.error):
                attempt += 1
                if attempt == RetryFTP.MAX_RETRIES + 1:
                    raise
                retry = True

    def set_on_retry(self, callback):
        self.on_retry = callback

    def getwelcome(self):
        return self.ftp.getwelcome()

    def retrbinary(self, cmd, callback, blocksize=8192, rest=None):
        self.__retrywrapper(FTP.retrbinary, cmd, callback, blocksize, rest)

    def storbinary(self, cmd, fp, blocksize=8192, callback=None, rest=None):
        self.__retrywrapper(FTP.storbinary, cmd, fp, blocksize, callback, rest)


# -----------------------------------------------------------------------------
# Download/upload functions
# -----------------------------------------------------------------------------
def self_update():
    print_status("Checking for script updates...")

    try:
        with request.urlopen(SCRIPT_URL) as response:
            # Download script from repo and compare version
            new_script = response.read().decode(response.headers.get_content_charset())
            for line in new_script.splitlines():
                result = re.match(
                    r"^SCRIPT_VERSION\s*=\s*\"([0-9]+.[0-9]+.[0-9]+)\"$", line
                )
                if result:
                    print_result("OK!", COLOR_GREEN)
                    if parse_version(result[1]) > parse_version(SCRIPT_VERSION):
                        # Overwrite self with new version
                        with open(
                            __file__, "w", encoding="utf-8", newline="\n"
                        ) as old_script:
                            old_script.write(new_script)

                        print("A new version of the script is available; respawning...")
                        pause(3)
                        restart()
                    else:
                        print("Script is up to date.\n")
                        return

            print_result("WARNING!", COLOR_YELLOW)
            print(
                "Unable to find version information in latest script from GitHub;"
                " continuing anyway...",
                file=sys.stderr,
            )

    except Exception:
        print_result("FAILED!", COLOR_RED)
        print("Failed to retrieve latest updater script from GitHub.", file=sys.stderr)


def get_current_version(ftp):
    result = re.search(r"mt32-pi (v[0-9]+.[0-9]+.[0-9]+)", ftp.getwelcome())
    if not result:
        print(
            "Failed to extract version number from FTP welcome message; continuing"
            " anyway",
            file=sys.stderr,
        )
        return "<unknown>"
    return result.group(1)


def get_old_data(ftp, temp_dir):
    ftp.set_on_retry(print_retry)

    for file_name in OLD_FILE_NAMES:
        with open(temp_dir / file_name, "wb") as file:
            print_status(f"Retrieving {COLOR_PURPLE}{file_name}{COLOR_RESET}...")
            try:
                ftp.retrbinary(f"RETR /SD/{file_name}", file.write)
                print_result("DONE!", COLOR_GREEN)
            except error_temp:
                print_result("NOT FOUND!", COLOR_YELLOW)

    return True


def get_latest_release_info():
    releases = []
    try:
        print_status("Retrieving release info from GitHub...")
        with request.urlopen(GITHUB_API_URL) as response:
            releases = json.load(response)
        print_result("OK!", COLOR_GREEN)
    except Exception:
        print_result("FAILED!", COLOR_RED)
        print("Failed to retrieve release info from GitHub.", file=sys.stderr)
        return None

    # Sort by version number in descending order
    releases.sort(reverse=True, key=lambda release: parse_version(release["tag_name"]))
    return releases[0]


def download_and_extract_release(release_info, destination_path):
    asset = release_info["assets"][0]
    url = asset["browser_download_url"]
    file_name = asset["name"]
    path = Path(destination_path) / file_name

    print_status(f"Downloading {COLOR_PURPLE}{file_name}{COLOR_RESET}...")

    try:
        with request.urlopen(url) as response, open(path, "wb") as out_file:
            length = response.getheader("content-length")
            block_size = 1024 * 1024

            if length:
                length = int(length)
                block_size = 8192

            buffer = io.BytesIO()
            done = 0
            while True:
                block = response.read(block_size)
                if not block:
                    break

                buffer.write(block)
                done += len(block)
                if length:
                    print_result(
                        f"[{(done / length * 100):>6.2f}%]",
                        replace=True,
                    )

            buffer.seek(0)
            shutil.copyfileobj(buffer, out_file)
            print_result("OK!", COLOR_GREEN)

        print_status(f"Unpacking {COLOR_PURPLE}{file_name}{COLOR_RESET}...")
        shutil.unpack_archive(path, Path(destination_path) / "install")
        print_result("OK!", COLOR_GREEN)
        return True

    except Exception as err:
        print_result("FAILED!", COLOR_RED)
        print(err)
        return False


# -----------------------------------------------------------------------------
# Config file processing functions
# -----------------------------------------------------------------------------
def find_section(lines, section):
    for index, line in enumerate(lines):
        if line.strip() == f"[{section}]":
            return index
    return None


def is_a_section(line):
    return re.match(r"^\[.+\]$", line.strip()) is not None


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
    while option_index + 1 < len(config_lines) and not is_a_section(
        config_lines[option_index]
    ):
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
        # print(f"Found {section}/{key} on line {option_index}")
        config_lines[option_index] = f"{key} = {value}"
    else:
        # print(f"Couldn't find {section}/{key}")
        insert_new_option_line(config_lines, section_index + 1, key, value)


def merge_configs(old_config_path, new_config_path, skipped_options):
    print_status("Merging your old settings into the new config file template...")

    try:
        config_old = ConfigParser()
        config_old.read(old_config_path)

        new_config_lines = []
        with open(new_config_path, "r") as in_file:
            new_config_lines = in_file.read().splitlines()

        for section in config_old.sections():
            # Don't merge deprecated sections
            if section.startswith("fluidsynth.soundfont."):
                skipped_options.append(f"[{section}] (whole section)")
                continue

            for item in config_old.items(section):
                key = item[0]
                value = item[1]

                # Don't merge deprecated options
                if section in DEPRECATED_OPTIONS and key in DEPRECATED_OPTIONS[section]:
                    skipped_options.append(key)
                    continue

                update_option(new_config_lines, section, key, value)

        with open(new_config_path, "w") as out_file:
            config_text = "\n".join(new_config_lines) + "\n"
            out_file.write(config_text)

        print_result("OK!", COLOR_GREEN)
        return True

    except Exception:
        print_result("FAILED!", COLOR_RED)
        return False


def install(ftp, source_path, config):
    ignore_list = [
        path.strip() for path in config.get(K_SECTION, K_IGNORE_LIST).split(",")
    ]

    filter_dirs = [Path(path[:-1]) for path in ignore_list if path.endswith("/")]
    filter_files = [Path(path) for path in ignore_list if not path.endswith("/")]

    for (dir_path, dir_names, filenames) in os.walk(source_path):
        remote_dir = Path(dir_path.replace(str(source_path), "").lstrip(os.sep))

        for file_name in filenames:
            local_file_path = Path(dir_path) / file_name
            remote_file_path = remote_dir / file_name
            print_status(f"Uploading {COLOR_PURPLE}{remote_file_path}{COLOR_RESET}...")

            if remote_dir in filter_dirs or remote_file_path in filter_files:
                print_result("SKIPPED!", COLOR_YELLOW)
                continue

            with open(local_file_path, "rb") as file:
                file.seek(0, os.SEEK_END)
                file_size = file.tell()
                file.seek(0)

                transferred = 0

                def callback(bytes):
                    nonlocal transferred
                    transferred += len(bytes)
                    print_result(
                        f"[{(transferred / file_size * 100):>6.2f}%]",
                        replace=True,
                    )

                def on_retry(attempt):
                    nonlocal transferred
                    transferred = 0
                    file.seek(0)
                    print_retry(attempt)

                ftp.set_on_retry(on_retry)
                ftp.storbinary(f"STOR /SD/{remote_file_path}", file, callback=callback)
                print_result("DONE!", COLOR_GREEN)

    return True


def show_release_notes(release_info):
    text = release_info["body"]

    # Poor man's Markdown formatting
    text = re.sub(
        r"\[(.+?)\]\(.+?\)", rf"{COLOR_PURPLE}\1{COLOR_RESET}", text, flags=re.MULTILINE
    )
    text = re.sub(
        r"^#+\s*(.+)$", rf"{COLOR_GREEN}\1{COLOR_RESET}", text, flags=re.MULTILINE
    )
    text = re.sub(
        r"^(\s*)-(.+)$", rf"\1{COLOR_PURPLE}-{COLOR_RESET}\2", text, flags=re.MULTILINE
    )
    text = re.sub(
        r"^(\s*)\*(.+)$", rf"\1{COLOR_PURPLE}*{COLOR_RESET}\2", text, flags=re.MULTILINE
    )
    text = re.sub(
        r"(\*\*|__)(.+)(\*\*|__)",
        rf"{COLOR_BRIGHT_WHITE}\2{COLOR_RESET}",
        text,
        flags=re.MULTILINE,
    )
    text = re.sub(
        r"`(.+?)`", rf"{COLOR_PURPLE}\1{COLOR_RESET}", text, flags=re.MULTILINE
    )

    date = datetime.strptime(
        release_info["published_at"], "%Y-%m-%dT%H:%M:%SZ"
    ).strftime("%Y-%m-%d")
    release_header = f"{release_info['tag_name']} - {date}"
    underline = "".join(["=" for i in range(0, len(release_header))])
    print(f"{COLOR_GREEN}{release_header}\n{underline}{COLOR_RESET}\n")

    print(text)
    pause()


def reboot(host):
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(bytes.fromhex("F0 7D 00 F7"), (host, 1999))


# -----------------------------------------------------------------------------
# Entry point
# -----------------------------------------------------------------------------
if __name__ == "__main__":
    print(CLEAR_TERM, end="")
    print(MT32PI_LOGO)

    # Load config defaults
    config = ConfigParser()
    config.read_dict(DEFAULT_CONFIG)

    # Load config file if available
    config_path = Path(__file__).with_suffix(".cfg").name
    config.read(config_path)

    if config.getboolean(K_SECTION, K_SELF_UPDATE):
        self_update()

    # Create temporary directory and get the latest release
    with tempfile.TemporaryDirectory() as temp_dir_name:
        temp_dir = Path(temp_dir_name)
        host = config.get(K_SECTION, K_HOST)
        skipped_options = []

        print_status(
            "Connecting to your mt32-pi's embedded FTP server at"
            f" '{COLOR_PURPLE}{host}{COLOR_RESET}'..."
        )
        try:
            with RetryFTP(
                host,
                config.get(K_SECTION, K_FTP_USERNAME),
                config.get(K_SECTION, K_FTP_PASSWORD),
                timeout=config.getfloat(K_SECTION, K_CONNECTION_TIMEOUT),
            ) as ftp:
                print_result("OK!", COLOR_GREEN)
                # ftp.set_debuglevel(2)

                current_version = get_current_version(ftp)
                release_info = get_latest_release_info() or exit(1)
                latest_version = release_info["tag_name"]

                print()
                print(
                    "The currently-installed version is:"
                    f" {COLOR_GREEN}{current_version}{COLOR_RESET}"
                )
                print(
                    "The latest release is:             "
                    f" {COLOR_GREEN}{latest_version}{COLOR_RESET}"
                )
                print()

                force_update = config.getboolean(K_SECTION, K_FORCE_UPDATE)
                if not force_update and parse_version(current_version) >= parse_version(
                    latest_version
                ):
                    print("Your mt32-pi is up to date.")
                    exit(0)

                if config.getboolean(K_SECTION, K_SHOW_RELEASE_NOTES):
                    show_release_notes(release_info)

                download_and_extract_release(release_info, temp_dir) or exit(1)

                # Fetch old configs
                get_old_data(ftp, temp_dir) or exit(1)

                install_dir = temp_dir / "install"
                old_config_path = install_dir / "mt32-pi.cfg.bak"
                new_config_path = install_dir / "mt32-pi.cfg"

                # Create backup of old config and merge with new
                shutil.move(temp_dir / "mt32-pi.cfg", old_config_path)
                merge_configs(
                    old_config_path, new_config_path, skipped_options
                ) or exit(1)

                # Move new Wi-Fi/boot configs aside in case the user needs to adapt them
                shutil.move(install_dir / "config.txt", install_dir / "config.txt.new")
                shutil.move(temp_dir / "config.txt", install_dir)

                old_wifi_config_path = temp_dir / "wpa_supplicant.conf"
                old_wifi_config_exists = old_wifi_config_path.exists()
                if old_wifi_config_exists:
                    shutil.move(
                        install_dir / "wpa_supplicant.conf",
                        install_dir / "wpa_supplicant.conf.new",
                    )
                    shutil.move(old_wifi_config_path, install_dir)

                # Upload new version
                install(ftp, install_dir, config) or exit(1)

        except socket.timeout:
            print_result("FAILED!", COLOR_RED)
            print_socket_timeout()
            exit(1)

        except socket.error:
            print_result("FAILED!", COLOR_RED)
            print_socket_error()
            exit(1)

        # Reboot mt32-pi
        reboot(host)

        print("\nAll done!\n")
        print(
            f"{COLOR_PURPLE}-{COLOR_RESET} The settings from your old config file have"
            " been merged into the latest config template."
        )
        if skipped_options:
            skipped_list = ", ".join(
                [f"{COLOR_PURPLE}{o}{COLOR_RESET}" for o in skipped_options]
            )
            print(
                f"{COLOR_PURPLE}-{COLOR_RESET} The following deprecated options were"
                f" removed from your config file: {skipped_list}"
            )
        print(
            f"{COLOR_PURPLE}-{COLOR_RESET} A backup of your old config is available as"
            f" {COLOR_PURPLE}mt32-pi.cfg.bak{COLOR_RESET} on the root of your Raspberry"
            " Pi's SD card."
        )
        print(
            f"{COLOR_PURPLE}-{COLOR_RESET} Your"
            f" {COLOR_PURPLE}config.txt{COLOR_RESET} has been preserved."
        )
        if old_wifi_config_exists:
            print(
                f"{COLOR_PURPLE}-{COLOR_RESET} Your"
                f" {COLOR_PURPLE}wpa_supplicant.conf{COLOR_RESET} has been preserved."
            )
        print(
            f"\n{COLOR_GREEN}Your mt32-pi should be automatically rebooting if UDP MIDI"
            f" is enabled. Otherwise, please power-cycle your mt32-pi.{COLOR_RESET}"
        )
