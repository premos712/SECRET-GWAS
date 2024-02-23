import hail as hl
import os
import shutil
import subprocess
from time import sleep


def send_data_to_SECRET(mt):
    mt.GT.n_alt_alleles().export('alleles-demo.tsv')
    mt.PurpleHair.export('PurpleHair.tsv')
    mt.isFemale.export('isFemale.tsv')
    shutil.move('alleles-demo.tsv', '../dpi/dpi_data/alleles-demo.tsv')
    shutil.move('PurpleHair.tsv', '../dpi/dpi_data/PurpleHair.tsv')
    shutil.move('isFemale.tsv', '../dpi/dpi_data/isFemale.tsv')

    os.chdir('../coordination_server/')
    os.system("make run &")

    sleep(.5)

    os.chdir('../dpi/')
    os.system("./bin/dpi configs/dpi_config-demo.json &")

def peform_logistic_regresion():
    os.chdir('../enclave_node/host')
    os.system("./gwashost configs/enclave_node_config-demo.json")

def export():
    os.chdir('../../hail_demo')
    shutil.move('../coordination_server/results.out', 'SECRET_results.vcf')
    print('Exported!')
    

