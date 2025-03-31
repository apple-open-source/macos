default: test lint

test:
	python test_ipaddress.py

lint:
	@(python --version 2>&1 | grep -q ' 2\.6\.') || flake8 ipaddress.py test_ipaddress.py

pypi:
	python setup.py sdist bdist_wheel upload

release:
	if test -z "${VERSION}"; then echo VERSION missing; exit 1; fi
	sed -i "s#^\(__version__\s*=\s*'\)[^']*'\$$#\1${VERSION}'#" ipaddress.py
	sed -i "s#^\(\s*'version':\s*'\)[^']*\('.*\)\$$#\1${VERSION}\2#" setup.py
	git diff
	git add ipaddress.py setup.py
	git commit -m "release ${VERSION}"
	git tag "v${VERSION}"
	git push --tags
	$(MAKE) pypi

clean:
	rm -rf -- build dist ipaddress.egg-info

.PHONY: default test clean pypi lint

