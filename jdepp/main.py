import os
import sys
import jagger
import argparse

def is_valid_model_file(parser, arg):
    if not os.path.isfile(arg):
        parser.error('The model file `{}` does not exist!'.format(arg))
    else:
        return arg

def main():
    parser = argparse.ArgumentParser("Python binding of J.DepP.")
    parser.add_argument('-m', '--model', metavar='FILE', type=lambda x: is_valid_model_file(parser, x),
      help="Path to model(dict) file.")

    args = parser.parse_args()

    print(args.model)

if __name__ == '__main__':
    main()
