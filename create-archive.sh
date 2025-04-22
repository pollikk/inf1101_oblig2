#!/bin/bash
# Author: Odin Bjerke, odin.bjerke@uit.no

ARCHIVE_SUFFIX=-"inf1101-p2"

# Directories to exclude from the archive
DIR_EXCLUDE_PAT=("data" "bin" "build" "debug" "local" "log" ".*")

# Files/extensions to exclude from the archive
FILE_EXCLUDE_PAT=("*.zip" "*.log" "__MACOSX" ".DS_Store")

confirm() {
  read -r -p "$1 [y/n]: " response
  if [[ "$response" == [Yy] ]]; then
    return 0
  else
    return 1
  fi
}

ARCHIVE_EXCLUDE=()

for dir in "${DIR_EXCLUDE_PAT[@]}"; do
  ARCHIVE_EXCLUDE+=("-x" "$dir/*" "-x" "$dir/**")
done

for pat in "${FILE_EXCLUDE_PAT[@]}"; do
  ARCHIVE_EXCLUDE+=("-x" "$pat" "-x" "**/$pat")
done

echo
echo "=== $(basename "$0") ==="
echo
echo "This script is provided for your convenience. It is used at your own risk."
echo "By continuing, you acknowledge that it is your responsibility alone to verify that all relevant files are"
echo "included in the created archive."
echo

if confirm "I have read and understood the above"; then
  read -r -p "Enter your UiT identifier (e.g. abc123): " uit_id
  echo "Running zip command. This might take a moment if your directory contains many files"

  zip -r "$uit_id$ARCHIVE_SUFFIX.zip" . "${ARCHIVE_EXCLUDE[@]}"

  if [ $? -eq 0 ]; then
    echo
    echo "=== Created $uit_id$ARCHIVE_SUFFIX.zip ==="
    echo "Extract your archive somewhere and verify that all your files are present, and that junk/data files are not."
  else
    echo "Interrupted."
  fi
else
  echo "Exiting."
  exit 1
fi
