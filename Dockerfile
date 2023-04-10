FROM archlinux:base

# Install dependencies (clang, xmake, git, make, cmake, ninja)
RUN pacman -Syu --noconfirm --needed clang xmake git make cmake ninja unzip && \
  useradd -m -G wheel -s /bin/bash cppdev

USER cppdev
WORKDIR /home/cppdev

# Install xmake
RUN xrepo add-repo gnussy https://github.com/gnussy/gnussy-repos
RUN xrepo install -y fmt prepucio tabulate gtest

# Copy the project files
COPY --chmod=0755 --chown=cppdev:cppdev . .

# Build the project
RUN xmake build -a
