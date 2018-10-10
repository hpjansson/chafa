#!/usr/bin/env python3.6
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
        call(['chafa', '-s', '80x24', '--symbols', 'block', im])
        print('------')
        call(['chafa', '-s', '80x24', '--symbols', 'custom', im])
        print('>>>>>> chafa: symbols=custom')
