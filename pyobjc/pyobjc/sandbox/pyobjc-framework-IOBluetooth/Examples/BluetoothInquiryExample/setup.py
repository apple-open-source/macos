from distutils.core import setup
import py2app

setup(
    name='BluetoothInquiryExample',
    app=["main.py"],
    data_files=["MainMenu.nib", 'BluetoothLogo.tiff'],
    options=dict(
        py2app=dict(
            iconfile='AppIcons.icns',
        ))
)
