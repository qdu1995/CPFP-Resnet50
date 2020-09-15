from __future__ import division
import numpy as np
import sys
caffe_root = '../caffe/'
sys.path.insert(0, caffe_root + 'python')
sys.path.insert(0, '/usr/bin/python')
sys.path.insert(0, '../caffe/lib/')

import caffe

def upsample_filt(size):
    factor = (size + 1) // 2
    if size % 2 == 1:
        center = factor - 1
    else:
        center = factor - 0.5
    og = np.ogrid[:size, :size]
    return (1 - abs(og[0] - center) / factor) * \
           (1 - abs(og[1] - center) / factor)

# set parameters s.t. deconvolutional layers compute  bilinear interpolation
# N.B. this is for deconvolution without groups
def interp_surgery(net, layers):
    for l in layers:
        print(l)
        m, k, h, w = net.params[l][0].data.shape
        if m != k:
            print( 'input + output channels need to be the same')
            
            raise
        if h != w:
            print ('filters need to be square')
            raise
        filt = upsample_filt(h)
        net.params[l][0].data[range(m), range(k), :, :] = filt

#base_weights = './vgg16_20M.caffemodel'
base_weights = './ResNet-50-model.caffemodel'
#base_weights = './_iter_6000.caffemodel.caffemodel'
# init
caffe.set_mode_gpu()
#caffe.set_device()



solver = caffe.SGDSolver('solver.prototxt')
solver.net.copy_from(base_weights)
solver.restore('../snapshot/_iter_14000.solverstate')




# do net surgery to set the deconvolution weights for bilinear interpolation
interp_layers = [k for k in solver.net.params.keys() if 'up' in k]
interp_surgery(solver.net, interp_layers)

# copy base weights for fine-tuning



solver.step(20000)
solver.net.save('./new/mymodel.caffemodel')