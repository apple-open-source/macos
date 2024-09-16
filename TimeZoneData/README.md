# TimeZoneData  
  
Radar Component: [ TZ Database | all ](radar://new/problem/componentId=248482)  
Source Location: <https://stash.sd.apple.com/projects/I18N/repos/TimeZoneData>  

Theory
------

The TimeZoneData project installs the tzcode and tzdata tar archives, as well as the latest data from ICU, into /usr/local/share/tz. It also soft-links the latest tzcode and tzdata archives so clients can find them.

The data is mastered into the internal SDKs, to be picked up by clients. Current clients are ICU, system\_cmds, and TimeZoneAsset. ICU and TimeZoneAsset can pick up the data from /usr/local/share/tz, while system\_cmds requires it to be in the SDK.

Release Procedure
-----------------

1. Subscribe to tz@iana.org, tz-announce@iana.org, and cldr-dev@googlegroups.com

2. When a new version of the TZ database is released by IANA, communicate with the CLDR/ICU team to see if any metazone or other changes are required.

3. If so, when the new ICU files are released to the ICU repository (<https://github.com/unicode-org/icu>), copy the following files into the icudata directory:

		icu4c/source/data/misc/metaZones.txt
		icu4c/source/data/misc/timezoneTypes.txt
		icu4c/source/data/misc/windowsZones.txt
		icu4c/source/tools/tzcode/icuregions
		icu4c/source/tools/tzcode/icuzones

4. Clone the tz repository from <https://github.com/eggert/tz>

5. Check out the tag corresponding to the release (e.g., 2020a)

6. Issue the following commands inside the repository:

		make traditional_tarballs
		make rearguard_tarballs
		cp tzcodeYYYYL.tar.gz (TimeZoneData directory)
		cp tzdataYYYYL-rearguard.tar.gz (TimeZoneData directory)/tzdataYYYYL.tar.gz

7. Inside TimeZoneData, remove the old tzcode and tzdata archives

8. Build a TimeZoneData root using buildit

9. cd to the root, and do:

		sudo ditto . /

10. Clone Apple ICU from <https://stashweb.sd.apple.com/projects/I18N/repos/icu/browse> and do:

		make check

11. There should be no errors in the tests. Further validation can be performed by producing a new version of TimeZoneAsset (<https://stashweb.sd.apple.com/projects/I18N/repos/timezoneasset/browse>), installing it, and performing a diff of all time zone transitions (see instructions in TimeZoneAsset)

12. Typically, TimeZoneData is only submitted to the build at the zero P2 deadline of a major OS release. The rest of the year we use OTA updates. However, every IANA release should get checked in.
