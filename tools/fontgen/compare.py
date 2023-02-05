#!/usr/bin/env python3
# Just a simple tool to compare the results using different symbol sets
# (C) 2018 Mo Zhou
# LGPLv3+
import glob
import argparse
import random
from subprocess import call

if __name__ == '__main__':
    ag = argparse.ArgumentParser()
    ag.add_argument('--glob', type=str, required=True)
    ag.add_argument('-n', type=int, default=32)
    ag = ag.parse_args()

    images = glob.glob(ag.glob, recursive=True)
    print(f'=> found {len(images)} images')

    for i in range(ag.n):
        im = random.choice(images)
        print('<<<<<< chafa: block')
        # FIXME: the command is oudated
        call(['chafa', '-s', '80x24', '--symbols', 'block', im])
        print('------')
        call(['chafa', '-s', '80x24', '--symbols', 'custom', im])
        print('>>>>>> chafa: symbols=custom')
