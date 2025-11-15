.PHONY: deps build test clean run docker-up docker-down help

# Default target
.DEFAULT_GOAL := help

# Build configuration
BUILD_TYPE ?= Release
BUILD_DIR := build
NPROC := $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

help: ## Show this help message
	@echo "TWS-Redis Bridge - Build Targets"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}'

deps: ## Install dependencies with Conan
	@echo "Installing dependencies..."
	conan install . --build=missing -s build_type=$(BUILD_TYPE) --output-folder=$(BUILD_DIR)

configure: deps ## Configure CMake project
	@echo "Configuring CMake..."
	cmake -B $(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_TOOLCHAIN_FILE=$(BUILD_DIR)/build/$(BUILD_TYPE)/generators/conan_toolchain.cmake \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

build: configure ## Build the project
	@echo "Building..."
	cmake --build $(BUILD_DIR) -j$(NPROC)

test: build ## Run tests
	@echo "Running tests..."
	cd $(BUILD_DIR) && ctest --output-on-failure

clean: ## Remove build artifacts
	@echo "Cleaning..."
	rm -rf $(BUILD_DIR)

run: build ## Build and run the bridge
	@echo "Running TWS bridge..."
	./$(BUILD_DIR)/tws_bridge

docker-up: ## Start Redis container
	@echo "Starting Redis..."
	docker-compose up -d

docker-down: ## Stop Redis container
	@echo "Stopping Redis..."
	docker-compose down

docker-logs: ## Show Redis logs
	docker-compose logs -f redis

redis-cli: ## Connect to Redis CLI
	docker exec -it tws-redis redis-cli

redis-monitor: ## Monitor Redis commands
	docker exec -it tws-redis redis-cli MONITOR

format: ## Format code (requires clang-format)
	find src include tests -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Development helpers
install-hooks: ## Install git hooks
	@echo "Installing git hooks..."
	@echo "#!/bin/bash\nmake format" > .git/hooks/pre-commit
	@chmod +x .git/hooks/pre-commit
