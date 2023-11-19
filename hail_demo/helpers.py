import hail as hl
import os
import shutil
import subprocess
from time import sleep


def send_data_to_SECRET(mt):
    mt.GT.n_alt_alleles().export('alleles-demo.tsv')
    mt.PurpleHair.export('PurpleHair.tsv')
    mt.isFemale.export('isFemale.tsv')
    shutil.move('alleles-demo.tsv', '../client/client_data/alleles-demo.tsv')
    shutil.move('PurpleHair.tsv', '../client/client_data/PurpleHair.tsv')
    shutil.move('isFemale.tsv', '../client/client_data/isFemale.tsv')

    os.chdir('../register_server/')
    os.system("make run &")

    sleep(.5)

    os.chdir('../client/')
    os.system("./bin/client configs/client_config-demo.json &")

def peform_logistic_regresion():
    os.chdir('../compute_server/host')
    os.system("./gwashost configs/compute_server_config-demo.json")


def export():
    os.chdir('../../hail_demo')
    shutil.move('../register_server/results.out', 'SECRET_results.vcf')
    print('Exported!')
    

