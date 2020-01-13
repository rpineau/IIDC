#!/bin/bash

mkdir -p ROOT/tmp/IIDC_X2/
cp "../IIDCCamera.ui" ROOT/tmp/IIDC_X2/
cp "../IIDCCamSelect.ui" ROOT/tmp/IIDC_X2/
cp "../cameralist IIDC.txt" ROOT/tmp/IIDC_X2/
cp "../build/Release/libIIDC.dylib" ROOT/tmp/IIDC_X2/

if [ ! -z "$installer_signature" ]; then
# signed package using env variable installer_signature
pkgbuild --root ROOT --identifier org.rti-zone.IIDC_X2 --sign "$installer_signature" --scripts Scripts --version 1.0 IIDC_X2.pkg
pkgutil --check-signature ./IIDC_X2.pkg
else
pkgbuild --root ROOT --identifier org.rti-zone.IIDC_X2 --scripts Scripts --version 1.0 IIDC_X2.pkg
fi

rm -rf ROOT
