#!/bin/sh
sudo echo "get the su permission"
echo "**********************************************"
echo "        building libion ..."
echo "**********************************************"
cd libion/
make install

echo ""
echo "**********************************************"
echo "        building libpost_deinterlace ..."
echo "**********************************************"
cd ../libvpu/post_process/deinterlace/
make install

echo ""
echo "**********************************************"
echo "        building libvpu ..."
echo "**********************************************"
cd ../../../libvpu/common/
make install

echo ""
echo "**********************************************"
echo "        building libvpu_avcdec ..."
echo "**********************************************"
cd ../../libvpu/avc_h264/dec/
make install

echo ""
echo "**********************************************"
echo "        building libvpu_avcenc ..."
echo "**********************************************"
cd ../enc/
make install

echo ""
echo "**********************************************"
echo "        building libOn2enc_common ..."
echo "**********************************************"
cd ../../../jpeghw/src_enc/common/
make install

echo ""
echo "**********************************************"
echo "        building libon2jpegenc ..."
echo "**********************************************"
cd ../jpeg/
make install

echo ""
echo "**********************************************"
echo "        building libjpeghwenc ..."
echo "**********************************************"
cd ../../release/encode_release/
make install

echo ""
echo "**********************************************"
echo "        building libOn2Dec_common ..."
echo "**********************************************"
cd ../../src_dec/common/
make install

echo ""
echo "**********************************************"
echo "        building libon2jpegdec ..."
echo "**********************************************"
cd ../jpeg/
make install

echo ""
echo "**********************************************"
echo "        building libjpeghwdec ..."
echo "**********************************************"
cd ../../release/decoder_release/
make install

echo ""
echo "**********************************************"
echo "        building librk_on2 ..."
echo "**********************************************"
cd ../../../libvpu/vpu_api/
make install

cd ../../
echo "  cp sharelib to /lib"
sudo cp sharelib/* /lib/
echo ""
echo " finish build !!!!!!"

