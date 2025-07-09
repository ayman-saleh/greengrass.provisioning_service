# Dockerfile for real AWS Greengrass provisioning test
FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    python3 \
    python3-pip \
    curl \
    wget \
    git \
    sqlite3 \
    sudo \
    systemd \
    systemd-sysv \
    openjdk-11-jre-headless \
    ca-certificates \
    jq \
    unzip \
    libcurl4-openssl-dev \
    libssl-dev \
    uuid-dev \
    zlib1g-dev \
    libpulse-dev \
    && rm -rf /var/lib/apt/lists/*

# Install AWS CLI v2
RUN curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip" && \
    unzip awscliv2.zip && \
    ./aws/install && \
    rm -rf awscliv2.zip aws/

# Install Conan 2.0 and Python dependencies
RUN pip3 install conan boto3 python-dotenv

# Create working directory
WORKDIR /workspace

# Copy source code
COPY . /workspace/

# Create necessary directories
RUN mkdir -p /greengrass/v2 /var/run /var/log

# Copy test scripts
COPY docker/real-test/scripts/ /scripts/
RUN chmod +x /scripts/*.sh

# Set environment variables
ENV PATH="/scripts:${PATH}"
ENV GREENGRASS_ROOT="/greengrass/v2"
ENV DB_PATH="/workspace/test-device.db"

# Entry point
ENTRYPOINT ["/scripts/run-real-test.sh"] 