
FROM debian:trixie

RUN apt-get update && apt-get install -y git

#######################################################################

RUN apt-get update && apt-get install -y \
     intltool g++  python3-docutils python3 \
     dbus  pkgconf  libopengl-dev  systemd-dev  systemd \
     mesa-vulkan-drivers libxrender-dev libxrandr-dev  libglx-dev \
     python3-packaging  libmount-dev libenchant-2-dev libgit2-dev \
     x11-common libglvnd-dev dconf-service  humanity-icon-theme \
     libgtk-3-dev  libvte-2.91-dev  gnupg  libfakeroot \
     build-essential autopoint libtool && \
     apt-get install -y libegl-mesa0 && \
     apt-get install -y autopoint automake autoconf \
         libtool gettext pkg-config \
         libgit2-dev libvte-2.91-dev libenchant-2-dev



# Force black background, who wants to look at a light bulb!
ENV GTK_THEME=Adwaita:dark
RUN apt-get install -y yaru-theme-gtk  xdg-desktop-portal-gtk
RUN gsettings set org.gnome.desktop.interface color-scheme prefer-dark


RUN mkdir /home/geany
WORKDIR /home/geany
ENV PATH="/sbin:/usr/sbin:${PATH}"
COPY .  /home/geany/
RUN ./autogen.sh
RUN make -j
RUN make install


WORKDIR /home
RUN git clone https://github.com/geany/geany-plugins.git
WORKDIR /home/geany-plugins
RUN sed -i 's/"Tree Browser"/"Folders"/g' treebrowser/src/treebrowser.c
RUN ./autogen.sh --enable-scope --enable-spellcheck --enable-gitchangebar
RUN make -j
RUN make install

CMD  printf 'usage\n podman run --net=host --env="DISPLAY" geany:latest\n' \
     && "geany"
