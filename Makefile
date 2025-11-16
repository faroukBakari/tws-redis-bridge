.PHONY: help deps configure build test clean run docker-up docker-down docker-logs redis-cli redis-monitor validate-env format install-hooks

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

validate-env: ## Validate development environment (Gate 1b)
	@echo "=== Validation Gate 1b: Development Environment ==="
	@echo ""
	@echo "Checking Redis container..."
	@if docker ps | grep -q redis; then \
		echo "✅ Redis container running"; \
	else \
		echo "❌ Redis container NOT running"; \
		echo "   Run: make docker-up"; \
		exit 1; \
	fi
	@echo ""
	@echo "Checking Redis connectivity..."
	@if docker exec tws-redis redis-cli PING 2>/dev/null | grep -q PONG; then \
		echo "✅ Redis responds to PING"; \
	else \
		echo "❌ Redis not responding"; \
		echo "   Check: docker-compose logs redis"; \
		exit 1; \
	fi
	@echo ""
	@echo "Checking TWS ports availability..."
	@if ! lsof -i :7497 >/dev/null 2>&1; then \
		echo "✅ Port 7497 (TWS live) available"; \
	else \
		echo "⚠️  Port 7497 in use (TWS may be running)"; \
	fi
	@if ! lsof -i :4002 >/dev/null 2>&1; then \
		echo "✅ Port 4002 (TWS paper) available"; \
	else \
		echo "⚠️  Port 4002 in use (TWS may be running)"; \
	fi
	@echo ""
	@echo "Checking bridge binary..."
	@if [ -f $(BUILD_DIR)/tws_bridge ]; then \
		echo "✅ Bridge binary exists: $(BUILD_DIR)/tws_bridge"; \
	else \
		echo "❌ Bridge binary NOT found"; \
		echo "   Run: make build"; \
		exit 1; \
	fi
	@echo ""
	@echo "✅ Validation Gate 1b: PASSED"
	@echo "Ready to proceed to Day 1 Afternoon (Gates 2a-2c)"

format: ## Format code (requires clang-format)
	find src include tests -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Development helpers
install-hooks: ## Install git hooks
	@echo "Installing git hooks..."
	@echo "#!/bin/bash\nmake format" > .git/hooks/pre-commit
	@chmod +x .git/hooks/pre-commit
