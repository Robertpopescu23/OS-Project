check_permissions() {
    local filename="$1"
    if [ ! -r "$filename" ]; then
        echo "File is not readable"
        exit 1
    fi
    if [ ! -w "$filename" ]; then
        echo "File is not writable"
        exit 2
    fi
    if [ ! -x "$filename" ]; then
        echo "File is not executable"
        exit 3
    fi
}

check_non_ascii() {
    local filename="$1"
    if grep -qP "[^\x00-\x7F]" "$filename"; then
        echo "Non-ASCII characters found in the file"
        exit 6
    fi
}

# Verify if the file is a shell script
if ! file "$1" | grep -q "shell script"; then
    echo "Not a shell script"
    exit 4
fi

# Check permissions of the file
check_permissions "$1"

# Check for non-ASCII characters in the file
check_non_ascii "$1"

# Check for specific patterns in the script content
if grep -qE "eval|\$\(|\$\{|bash|curl|wget" "$1"; then
    echo "Potentially malicious patterns found"
    exit 5
fi



echo "No malicious patterns found"
exit 0