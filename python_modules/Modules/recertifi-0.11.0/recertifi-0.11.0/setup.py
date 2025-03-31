import os
from setuptools import setup
from setuptools.command.sdist import sdist as SdistCommand

from recertifi import version

with open("README.md") as f:
    readme = f.read()


class CustomSdistCommand(SdistCommand):

    def run(self):
        SdistCommand.run(self)
        self.run_command("bdist_wheel")


class CustomBdistCommand(object):
    """Override the Bdist command in a way that the wheel module is not required to be installed.
    """

    def __init__(self, *args, **kwargs):
        # Use composition instead of inheritance so the wheel import is not needed until after
        # the setup() function is called (at which point the setup_requires() will have been
        # installed).
        from wheel.bdist_wheel import bdist_wheel
        self.orig_bdist_command = bdist_wheel(*args, **kwargs)

    def __getattr__(self, name):
        """Delegate any call to methods in this object to the original bdist_wheel command.
        """
        return getattr(self.orig_bdist_command, name)

    def ensure_finalized(self):
        self.orig_bdist_command.ensure_finalized()
        # Override for Rio
        if os.environ.get("CI", False):
            self.orig_bdist_command.dist_dir = os.path.join(os.path.dirname(__file__),
                                                            ".cicd/lib/python")
            # Since we're not calling the "bdist_wheel" the setup.cfg doesnt get parsed
            self.orig_bdist_command.universal = True


setup(name="recertifi",
      version=version.__version__,
      description="Slim utility package to add the Apple Root Certificate Authorities to the certifi store",
      long_description=readme,
      url="https://github.pie.apple.com/michael-r-carroll/recertifi",
      author="Michael Carroll",
      author_email="mrc@apple.com",
      license="Apple Internal",
      packages=["recertifi", "recertifi_cli"],
      setup_requires=["wheel"],
      install_requires=[],
      cmdclass={
          "bdist_wheel": CustomBdistCommand,
          "sdist": CustomSdistCommand
      },
      zip_safe=False,
      include_package_data=True,
      test_suite="nose.collector",
      tests_require=["nose",
                     "parameterized",
                     "requests"]
      )
