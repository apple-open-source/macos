This folder has the scripts and schemas for fetching the QWAC anchors from the EU List Of Trusted Lists.

The schemas and code were implemented against v.2.2.1.
The trusted list documentation is https://www.etsi.org/deliver/etsi_ts/119600_119699/119612/02.02.01_60/ts_119612v020201p.pdf
The schemas are in https://www.etsi.org/deliver/etsi_ts/119600_119699/119612/02.02.01_60/ts_119612v020201p0.zip


The EU LOTL signing certificates embedded in the LOTL...the published ones to comare against are out of date.

```
python3 -m pip install --user jsonschema
python3 -m pip install --user lxml
python3 -m pip install --user signxml
```
