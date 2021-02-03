#!/usr/bin/env python3

from discord_webhook import DiscordWebhook, DiscordEmbed
from sys import argv
import json
import keepachangelog

GITHUB_USER = "dwhinham"
GITHUB_REPO = "mt32-pi"

RAW_URL = "https://raw.githubusercontent.com/{}/{}/master".format(
    GITHUB_USER, GITHUB_REPO
)
AVATAR_URL = "{}/images/mt32pi_logo_discord_avatar.png".format(RAW_URL)
THUMBNAIL_URL = "{}/images/mt32pi_logo_dark.png".format(RAW_URL)

MAX_FIELD_LEN = 1024
COLOR = 0xEB2188


def add_field(embed, changes, field_title, section_name):
    if section_name in changes:
        value = "\n".join(["‣ {}".format(change) for change in changes[section_name]])
        if len(value) > MAX_FIELD_LEN:
            value = value[: MAX_FIELD_LEN - 3] + "..."

        embed.add_embed_field(name=field_title, value=value, inline=False)


if len(argv) < 3:
    print("Usage: {} [webhook url] [version]".format(argv[0]))
    exit(1)

webhook_url = argv[1]
version = argv[2]

changelog = keepachangelog.to_dict("./CHANGELOG.md")
if version not in changelog:
    print("Version {} not in changelog!".format(version))
    exit(1)

changes = changelog[version]

title = "🎹🎶 **{} v{} - {}**".format(
    GITHUB_REPO, changes["version"], changes["release_date"]
)

release_url ="https://github.com/{}/{}/releases/tag/v{}".format(GITHUB_USER, GITHUB_REPO, version)


webhook = DiscordWebhook(url=webhook_url, username=GITHUB_REPO, avatar_url=AVATAR_URL)
embed = DiscordEmbed(title=title, url=release_url, color=COLOR)
embed.set_author(
    name=GITHUB_USER,
    url="https://github.com/{}".format(GITHUB_USER),
    icon_url="https://github.com/{}.png".format(GITHUB_USER),
)
embed.set_thumbnail(url=THUMBNAIL_URL)

add_field(embed, changes, "✨ Added:", "added")
add_field(embed, changes, "✏ Changed:", "changed")
add_field(embed, changes, "🐛  Fixed:", "fixed")

webhook.add_embed(embed)
response = webhook.execute()
