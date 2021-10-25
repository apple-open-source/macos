#!/usr/bin/python3
import tarfile
import sqlite3
import sys
import csv
import os

"""
    This is a utility script that extracts PLBatteryAgent_EventBackward_Battery information 
    from a sysdiagnose tarfile or folder as CSV files.
    Usage: ./getPowerlogCSV.py <pathToysdiagnoseFolderOrTarFile>
"""
def main():
    if len(sys.argv) < 1:
        print('Specify the tarfile or the sysdiagnose directory!')
    powerlogs = []
    if os.path.isdir(sys.argv[1]):
        path = sys.argv[1]
        for root, directories, files in os.walk(path):
            if 'logs/powerlogs' in root:
                for file in files:
                    powerlogs.append(os.path.join(root, file))
    elif tarfile.is_tarfile(sys.argv[1]):
        with tarfile.open(sys.argv[1], mode="r") as tf:
            members = tf.getmembers()
            for member in members:
                if 'logs/powerlogs' in member.name:
                    powerlogs.append(member)
            tf.extractall(members=powerlogs)

    for powerlog in powerlogs:
        if os.path.isdir(sys.argv[1]):
            powerlogName = powerlog
        elif tarfile.is_tarfile(sys.argv[1]):
            powerlogName = powerlog.name

        with sqlite3.connect(powerlogName) as conn:
            cursor = conn.cursor()
            cursor.execute(
                "select * FROM PLBatteryAgent_EventBackward_Battery")
            header = [member[0] for member in cursor.description]
            csvFileName = powerlogName.split('/')[-1].split('.')[0]
            csvFileName += '.csv'

            with open(csvFileName, 'w') as csvfile:
                csvwriter = csv.writer(csvfile, delimiter=',')
                csvwriter.writerow(header)
                rows = cursor.fetchall()
                for row in rows:
                    csvwriter.writerow(list(row))


if __name__ == '__main__':
    main()
