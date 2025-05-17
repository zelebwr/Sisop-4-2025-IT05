FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Jakarta

# Install dependencies + timezone + libfuse3-dev for fuse3
RUN apt-get update && apt-get install -y \
    tzdata \
    fuse3 \
    libfuse3-dev \
    gcc \
    pkg-config \
    wget \
    && ln -fs /usr/share/zoneinfo/$TZ /etc/localtime \
    && dpkg-reconfigure -f noninteractive tzdata \
    && rm -rf /var/lib/apt/lists/*

# Create folder for FUSE mount and original files
RUN mkdir -p /original_file

# Download the images into /original_file
RUN wget --no-check-certificate 'https://docs.google.com/uc?export=download&id=1_lEz_pV3h4uippLfOLeqO1bQ5bM8a1dy' -O /original_file/nafis.jpg && \
    wget --no-check-certificate 'https://docs.google.com/uc?export=download&id=18R58eAfJ-1xkgE57WjzFgc16w2UdFlBU' -O /original_file/kimcun.jpg

# Copy source code
COPY antink.c /antink.c

# Build the FUSE program
RUN gcc -D_FILE_OFFSET_BITS=64 -Wall `pkg-config fuse3 --cflags --libs` antink.c -o antink

CMD ["./antink", "/mnt"]
