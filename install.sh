#!/bin/bash

# Copyright (c) 2025 David Bertet. Licensed under the MIT License.

# This script checks for Node.js and then runs the main CLI tool
# It forwards all command line arguments to the Node.js script

# Colors for terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Print header
echo -e "${GREEN}${BOLD}"
echo "┌─────────────────────────┐"
echo "│     RainMaker CLI       │"
echo "│   Setup & Deploy Tool   │"
echo "└─────────────────────────┘"
echo -e "${NC}"

# Function to print colored messages
log_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

log_success() {
    echo -e "${GREEN}✓${NC} $1"
}

log_error() {
    echo -e "${RED}✗${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

log_step() {
    echo -e "${CYAN}${BOLD}🚀 $1${NC}"
}

print_separator() {
    echo -e "${MAGENTA}$(printf '─%.0s' {1..60})${NC}"
}

# Show usage information
show_usage() {
cat << 'EOF'
Usage: ./install.sh [OPTIONS]

Options:
  -o, --ota <IP>              Set OTA IP address for over-the-air updates
  -p, --upload-password <PWD> Set upload password for authentication
  -y, --yes                   Auto-confirm all prompts (non-interactive mode)
  -f, --frontend-only         Build/deploy frontend only
  -b, --backend-only          Build/deploy backend only
  -h, --help                  Show this help message

Examples:
  ./install.sh -o 192.168.1.100
  ./install.sh --ota 192.168.1.100 --upload-password mypass123
  ./install.sh -o 192.168.1.100 -p mypass123 -y
  ./install.sh -f -y
  ./install.sh --backend-only --yes

Notes:
  - Use -f or -b to target specific components
  - Use -y to skip confirmation prompts
  - OTA IP should be a valid IPv4 address
EOF
}

# Parse command line arguments
ARGS=("$@")

# Check for help flag
if [[ " $* " == *" --help "* ]] || [[ " $* " == *" -h "* ]]; then
    show_usage
    exit 0
fi

# Check if running on supported OS
check_os() {
    case "$(uname -s)" in
        Linux*)     OS="Linux";;
        Darwin*)    OS="macOS";;
        CYGWIN*|MINGW*|MSYS*) OS="Windows";;
        *)          OS="Unknown";;
    esac
    
    log_info "Detected OS: $OS"
}

# Check if Node.js is installed
check_node() {
    log_step "Checking for Node.js..."
    print_separator
    
    if command -v node >/dev/null 2>&1; then
        NODE_VERSION=$(node --version 2>/dev/null)
        log_success "Node.js found ($NODE_VERSION)"
        
        if command -v npm >/dev/null 2>&1; then
            NPM_VERSION=$(npm --version 2>/dev/null)
            log_success "npm found ($NPM_VERSION)"
            return 0
        else
            log_error "npm not found (but Node.js is installed)"
            show_npm_install_instructions
            return 1
        fi
    else
        log_error "Node.js not found"
        
        show_node_install_instructions
        return 1
    fi
}

# Show Node.js installation instructions
show_node_install_instructions() {
    echo
    echo -e "${YELLOW}Please install Node.js first:${NC}"
    echo
    
    case $OS in
        "macOS")
            echo "Option 1 - Official installer:"
            echo "  Visit: https://nodejs.org/"
            echo
            echo "Option 2 - Using Homebrew:"
            echo "  brew install node"
            echo
            echo "Option 3 - Using MacPorts:"
            echo "  sudo port install nodejs18"
            ;;
        "Linux")
            echo "Option 1 - Official installer:"
            echo "  Visit: https://nodejs.org/"
            echo
            echo "Option 2 - Using package manager:"
            echo "  Ubuntu/Debian: sudo apt update && sudo apt install nodejs npm"
            echo "  CentOS/RHEL:   sudo yum install nodejs npm"
            echo "  Fedora:        sudo dnf install nodejs npm"
            echo "  Arch Linux:    sudo pacman -S nodejs npm"
            echo
            echo "Option 3 - Using NodeSource repository:"
            echo "  curl -fsSL https://deb.nodesource.com/setup_lts.x | sudo -E bash -"
            echo "  sudo apt-get install -y nodejs"
            ;;
        "Windows")
            echo "Option 1 - Official installer:"
            echo "  Visit: https://nodejs.org/"
            echo "  Download and run the Windows installer"
            echo
            echo "Option 2 - Using Chocolatey:"
            echo "  choco install nodejs"
            echo
            echo "Option 3 - Using Scoop:"
            echo "  scoop install nodejs"
            ;;
        *)
            echo "Visit: https://nodejs.org/"
            echo "Download and install Node.js for your operating system"
            ;;
    esac
    
    echo
    log_warning "After installing Node.js, please restart your terminal and run this script again."
}

# Show npm installation instructions (rare case)
show_npm_install_instructions() {
    echo
    log_warning "Node.js is installed but npm is missing."
    echo -e "${YELLOW}To install npm:${NC}"
    echo
    
    case $OS in
        "macOS")
            echo "  brew install npm"
            echo "  or reinstall Node.js from https://nodejs.org/"
            ;;
        "Linux")
            echo "  sudo apt install npm          # Ubuntu/Debian"
            echo "  sudo yum install npm          # CentOS/RHEL"
            echo "  sudo dnf install npm          # Fedora"
            echo "  sudo pacman -S npm            # Arch Linux"
            ;;
        "Windows")
            echo "  Reinstall Node.js from https://nodejs.org/"
            echo "  (npm should be included with Node.js)"
            ;;
        *)
            echo "  Reinstall Node.js from https://nodejs.org/"
            ;;
    esac
}

# Main execution
main() {
    check_os
    echo
        
    if check_node; then        
        # Forward all arguments to the Node.js script
        if [[ ${#ARGS[@]} -gt 0 ]]; then
            echo
            node cli/index.js "${ARGS[@]}"
        else
            node cli/index.js
        fi
    else
        echo
        log_error "Installation cannot continue without Node.js."
        exit 1
    fi
}

# Handle Ctrl+C gracefully
trap 'echo -e "\nInstallation cancelled by user."; exit 130' INT

# Run main function with all arguments
main "$@"