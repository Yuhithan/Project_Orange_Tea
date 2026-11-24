
FROM randomdude/gcc-cross-x86_64-elf

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
        curl \
        build-essential \
        grub-common \
        grub-pc-bin \
        xorriso \
        nasm \
        cmake \
        git \
        make \
        pkg-config \
        ca-certificates \
        qemu-system-x86 \
        mtools \
    && rm -rf /var/lib/apt/lists/*

# Install Rust
ENV RUSTUP_HOME=/root/.rustup
ENV CARGO_HOME=/root/.cargo
ENV PATH="/root/.cargo/bin:${PATH}"

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
    sh -s -- -y --profile minimal --default-toolchain stable

# Install Rust components and target
RUN rustup component add rust-src llvm-tools rustfmt clippy && \
    rustup target add x86_64-unknown-none

# Set default toolchain
RUN rustup default stable

# Environment directory
VOLUME ["/root/env"]
WORKDIR /root/env

CMD ["/bin/bash"]