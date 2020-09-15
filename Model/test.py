import numpy as np
from PIL import Image
import os
import sys
import cv2
import logging

caffe_root = '../caffe/'
sys.path.insert(0, caffe_root + 'python')
sys.path.insert(0, caffe_root + '/lib/')

import shutil
import caffe
import time

EPSILON = 1e-8

logging.basicConfig(level=logging.DEBUG, format='%(asctime)s %(filename)s [line:%(lineno)d] %(levelname)s %(message)s',
                    datefmt='%a, %d %b %Y %H:%M:%S', filename='result.log', filemode='w')

console = logging.StreamHandler()
console.setLevel(logging.INFO)
formatter = logging.Formatter('%(name)-12s: %(levelname)-8s [line:%(lineno)d] %(message)s')
console.setFormatter(formatter)
logging.getLogger('').addHandler(console)


def load_image(im_name):
    im = cv2.imread(im_name)

    im = np.array(im, dtype=np.float32)
    height = im.shape[0]
    width = im.shape[1]
    im = im[:, :, ::-1]
    im -= np.array((104.00698793, 116.66876762, 122.67891434))
    im = im.transpose((2, 0, 1))
    return im, height, width


def caffe_forward(caffemodel, deploy_file, test_lst, out_lst, crf_lst):
    logging.info('Beginning caffe_forward...')
    caffe.set_mode_gpu()
    caffe.set_device(0)
    caffe.SGDSolver.display = 0
    net = caffe.Net(deploy_file, caffemodel, caffe.TEST)

    num_right = 0
    num_wrong = 0
    time_t = 0.0
    for i in range(len(test_lst)):
        im_name = test_lst[i]
        out_name = out_lst[i]
        print im_name
        #  print(sp_name)
        im, height, width = load_image(im_name)

        time_start = time.time()
        net.forward()
        time_end = time.time()
        time_t = time_t + time_end - time_start

        res = net.blobs['sigmoid_fuse'].data[0][0, :, :]
        res = np.array(res * 255, dtype=np.uint8)
        res = cv2.resize(res, (width, height))
        cv2.imwrite(out_name, res)
        if (i + 1) % 50 == 0:
            line = 'Processed {} images'.format(i + 1)
            logging.info(line)
    print('time cost is %s', time_t)
    print('average time cost is %s', time_t / len(test_lst))


def load_test_data(data_folder, test_lst, mode=1):
    logging.info('Beginning loading dataset...')
    im_lst = []
    label_lst = []
    im_name = []
    gt_lst = []
    sal_lst = []
    crf_lst = []
    out_lst = []
    im_folder = data_folder
    gt_folder = data_folder
    if mode == 1:
        sal_folder = data_folder + '/Result/'
        crf_folder = data_folder + '/crf/'
        with open(test_lst) as f:
            lines = f.readlines()
            for i in range(len(lines)):
                line = lines[i]
                line = line[:-1]
                name = line.split()[0][:-4]

                im_lst.append(im_folder + 'RGB/' + name + '.jpg')
                out_lst.append(im_folder + 'results6000/' + name + '.jpg')

                gt_lst.append(gt_folder + '/GT/' + name[5:] + '.png')
                sal_lst.append(sal_folder + name[5:] + '.png')
                crf_lst.append(crf_folder + name[5:] + '.png')
    return im_lst, gt_lst, sal_lst, crf_lst, out_lst

if __name__ == '__main__':
    if len(sys.argv) == 1:
        print 'Usage: {} iters dataset'.format(sys.argv[0])

    caffemodel = '../snapshot/_iter_6000.caffemodel'
    #caffemodel = '../final.caffemodel'
    deploy_file = './test.prototxt'
    data_folder = '../data/LFSD/'
    im_lst, gt_lst, sal_lst, crf_lst, out_lst= load_test_data(data_folder,data_folder+'list.lst')


    caffe_forward(caffemodel, deploy_file, im_lst, out_lst, crf_lst)

    crf = False
    if crf:
        crf_labeling(im_lst, sal_lst, crf_lst)
        # do_eval(crf_lst, gt_lst, iters, label_lst, label_res)
        salmetric.do_evaluation(8, crf_lst, gt_lst)