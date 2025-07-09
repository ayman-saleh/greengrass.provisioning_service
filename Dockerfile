# Build stage
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    python3 \
    python3-pip \
    git \
    libcurl4-openssl-dev \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Conan
RUN pip3 install conan

# Set up working directory
WORKDIR /build

# Copy source code
COPY conanfile.py CMakeLists.txt ./
COPY src/ ./src/
COPY tests/ ./tests/
COPY scripts/ ./scripts/

# Build the project
RUN mkdir build && cd build && \
    conan install .. --build=missing && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake && \
    make -j$(nproc)

# Runtime stage
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libcurl4 \
    libsqlite3-0 \
    sudo \
    systemd \
    systemd-sysv \
    openjdk-11-jre-headless \
    net-tools \
    iputils-ping \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Copy built binary
COPY --from=builder /build/build/greengrass_provisioning_service /usr/local/bin/

# Copy systemd service file
COPY scripts/greengrass-provisioning.service /etc/systemd/system/

# Create directories
RUN mkdir -p /opt/greengrass/config \
    /greengrass/v2 \
    /var/log \
    /var/run

# Set up systemd
RUN systemctl enable greengrass-provisioning.service

# Test stage
FROM runtime AS test

# Install test dependencies
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    netcat \
    jq \
    && rm -rf /var/lib/apt/lists/*

# Install Python test dependencies
RUN pip3 install pytest requests boto3 docker

# Copy test scripts and data
COPY docker/test/ /test/
COPY docker/test-data/ /test-data/

# Set working directory
WORKDIR /test

# Default command runs tests
CMD ["python3", "-m", "pytest", "-v", "integration_tests.py"] 