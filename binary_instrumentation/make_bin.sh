set +x
SRC=$1
PROG_NAME=$2
mkdir -p obj-ia32
PIN_DIR=/home/sekar/workspace/hw4/pin-3.6-97554-g31f0a167d-gcc-linux/
printf "src ${SRC}  cmd: make PIN_ROOT=${PIN_DIR} obj-ia32/${SRC} TARGET=ia32 \n"
make PIN_ROOT=${PIN_DIR} obj-ia32/${SRC} TARGET=ia32

#pin -t obj-ia32/${SRC} -- ${PROG_NAME}
