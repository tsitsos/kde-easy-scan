$EXTRACTRC `find . -name \*.ui` >>  rc.cpp
$XGETTEXT *.cpp -o $podir/kEasySkan.pot
rm -f rc.cpp
