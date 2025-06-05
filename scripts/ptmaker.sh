#!/bin/bash

show_help() {
	cat <<EOF
Usage: $(basename "$0") [--help] flash-size [tflite-file] [--no_factory]

The flash size should be provided in bytes, kilobytes, megabytes or
gigabytes. For example, for 1MB flash size:

$(basename "$0") 1073741824
or
$(basename "$0") 1024KB
or
$(basename "$0") 1MB

This script calculates the maximum equal size for 3 app partitions and
generates a partition table based on the given flash size. It also offers
the option to include a TFLite model partition if the path to a model file
is provided as a second argument. Finally, there is a third argument
to disable the factory partition which results in the generation of only
2 app partitions.

Each image starts at a 64KB-aligned offset.

The model partition is of type data and starts at a 4KB-aligned offset.

Output:
  - Optimal partition table (partitions properly aligned)
EOF
}

if [[ "$1" == "--help" ]]; then
	show_help
	exit 0
fi

# Default to 8MB if no argument is provided
input_size="$1"
if ! echo "$input_size" | grep -E -qi '^[0-9]+([KMG]?B)?$'; then
	>&2 echo "# Error: Invalid flash size format. Use formats like 8, 4KB, 2MB, 1GB"
	exit 1
fi

value=$(echo "$input_size" | grep -o -E '^[0-9]+')

unit=$(echo "$input_size" | grep -o -E '[KMG]?B' | tr '[:lower:]' '[:upper:]')
if [[ -z "$unit" ]]; then
    unit="B"
fi

# Determine multiplier
case "$unit" in
    B)
        multiplier=1
        ;;
    KB)
        multiplier=$((1024))
        ;;
    MB)
        multiplier=$((1024 * 1024))
        ;;
    GB)
        multiplier=$((1024 * 1024 * 1024))
        ;;
    *)
        >&2 echo "# Error: Unknown unit '$unit'"
        exit 1
        ;;
esac

flash_size=$((value * multiplier))
align=$((0x10000))
app_off=$((0x20000))

# Check if the second argument exists and if it does it should be an existent tflite file
if [[ -n "$2" ]]; then
	tflite_file="$2"
	if [[ ! -f "$tflite_file" ]]; then
		>&2 echo "# Error: TFLite file '$tflite_file' does not exist."
		exit 1
	fi
else
	tflite_file=""
fi

# Allocate space for a tflite model partition if requested 
if [[ -n "$tflite_file" ]]; then
	tflite_size=$(stat -c %s "$tflite_file")

	# Align the tflite size to 4KB i.e. round up to the closest multiple of 4KB
	tflite_aligned_size=$(((tflite_size + 0xFFF) & ~0xFFF ))
	
	# Ensure there is enough space for the tflite model
	tflite_off=$(($flash_size - $tflite_aligned_size))
	if (( tflite_off < app_off )); then
		>&2 echo "# Error: TFLite file size exceeds available space."
		exit 1
	fi

	# Ensure the tflite offset is 4KB aligned
	if (( tflite_off % 0x1000 != 0 )); then
		>&2 echo "# Error: TFLite offset $tflite_off is not 4KB aligned."
		exit 1
	fi

	avail=$(($tflite_off - $app_off))
else
	tflite_aligned_size=0
	tflite_off=0
	avail=$(($flash_size - $app_off))
fi

# Check if there is enough space left for the app partitions
if (( avail < 0 )); then
	>&2 echo "# Error: Flash size is too small for the given TFLite file."
	exit 1
fi

# Check if the third argument is provided (in this case, it should be --no_factory)
if [[ -n "$3" ]]; then
	if [[ "$3" == "--no_factory" ]]; then
		num_apps=$((2))
	else
		>&2 echo "# Error: Unknown option '$3'. Use --no_factory to disable factory partition."
		exit 1
	fi
else
	num_apps=$((3))
fi

# The maximum size for each app partition is half of the available space
max_image_size=$((avail / num_apps))
aligned_image_size=$(( (max_image_size / align) * align ))

image1_off=$app_off
image2_off=$((app_off + aligned_image_size))
if (( num_apps == 3 )); then
	image3_off=$((app_off + 2 * aligned_image_size))
else
	image3_off=$((0))
fi

# Ensure the image offsets are 64KB aligned
for addr in $image1_off $image2_off $image3_off; do
    if (( addr % align != 0 )); then
        >&2 echo "# Error: Image start offset $addr is not 64KB aligned."
        exit 1
    fi
done

apps_size=$(printf "0x%X" "$aligned_image_size")

if (( num_apps == 3 )); then
	app_off=$(printf "0x%X" "$image1_off")
	ota0_off=$(printf "0x%X" "$image2_off")
	ota1_off=$(printf "0x%X" "$image3_off")
else
	ota0_off=$(printf "0x%X" "$image1_off")
	ota1_off=$(printf "0x%X" "$image2_off")
fi

comments="# Name,       Type, SubType, Offset,   Size,    Flags"
nvs_entry="nvs,          data, nvs,     0x11000,  0x6000,"
phy_entry="phy_init,     data, phy,     0x17000,  0x1000,"
ota_entry="otadata,      data, ota,     0x18000,  0x2000,"
ota0_entry="ota_0,        app,  ota_0,   $ota0_off, $apps_size,"
ota1_entry="ota_1,        app,  ota_1,   $ota1_off, $apps_size,"

echo $comments
echo $nvs_entry
echo $phy_entry
echo $ota_entry

# Add the factory partition if requested
if (( num_apps == 3 )); then
	app_entry="factory,      app,  factory, $app_off,  $apps_size,"
	echo $app_entry
fi

echo $ota0_entry
echo $ota1_entry

# Add the TFLite model partition if requested
if (( tflite_aligned_size > 0 )); then
	model_size=$(printf "0x%X" "$tflite_aligned_size")
	model_off=$(printf "0x%X" "$tflite_off")
	tflite_entry="tflite_model, data, spiffs, $model_off, $model_size,"
	echo $tflite_entry
fi