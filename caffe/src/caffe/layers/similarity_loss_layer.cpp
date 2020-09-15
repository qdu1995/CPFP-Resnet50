#include <algorithm>
#include <cmath>
#include <vector>

#include "caffe/layers/similarity_loss_layer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {

// bottom[0]: superpixel pooled feature map    1 * 1 * S * 64
// bottom[1]: superpixel segmented labels      1 * 1 * 1 * S
// bottom[2]: superpixel label

template <typename Dtype>
inline Dtype sigmoid2(Dtype x) {
  return 0.5 * tanh(-0.5 * x) + 0.5;
}

template <typename Dtype>
void SimilarityLossLayer<Dtype>::LayerSetUp(
    const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
  LossLayer<Dtype>::LayerSetUp(bottom, top);
  const SimilarityLossParameter& similarity_loss_param = this->layer_param_.similarity_loss_param();
  //CHECK((similarity_loss_param.has_sample_points()))
  //    << "Either axis or concat_dim should be specified; not both.";
  num_per_region_ = similarity_loss_param.sample_points();
}

template <typename Dtype>
void SimilarityLossLayer<Dtype>::Reshape(
    const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
  LossLayer<Dtype>::Reshape(bottom, top);
  int num_ = bottom[0]->shape(0);
  height_ = bottom[0]->shape(1);
  width_ = bottom[0]->shape(2);
  numfeat_ = bottom[0]->shape(3);
  const Dtype* bottom_data = bottom[0]->cpu_data();
  transpose_bottom.Reshape(bottom[0]->shape(0), height_, width_, numfeat_);
  Dtype* transpose_bottom_data = transpose_bottom.mutable_cpu_data();
  
  for (int n = 0; n < num_; n++){
  for (int h = 0; h < height_; h++)
  {
  for (int w = 0; w < width_; w++)
  {
  for (int c = 0; c < numfeat_; c++)
  {
  transpose_bottom_data[n * height_ * width_ * numfeat_ + h * width_ * numfeat_ + w * numfeat_ + c] = bottom_data[n * height_ * width_ * numfeat_ + c * width_ * height_ + h * width_ + w];
 // LOG(INFO)<<transpose_bottom_data[n * height_ * width_ * numfeat_ + h * width_ * numfeat_ + w * numfeat_ + c];
  }
  }
  }
  }
  numpixel = height_ * width_;
 // CHECK((bottom[0]->num() == 1) && (bottom[1]->num() == 1))
 //     << "Bacth size must be equal to 1.";
 
}

template <typename Dtype>
void SimilarityLossLayer<Dtype>::Forward_cpu(
    const vector<Blob<Dtype>*>& bottom, const vector<Blob<Dtype>*>& top) {
    all_labels.clear();
    const Dtype* bottom_data = transpose_bottom.cpu_data();
    const Dtype* bottom_label = bottom[1]->cpu_data();
   

    const int dx4[4] = {-1, 0, 1, 0};
  	const int dy4[4] = { 0,-1, 0, 1};
    for(int i = 0; i < bottom[0]->num(); i++) {
      vector<vector<int> > adjacent(numpixel);
      for(int j = 0; j < height_; j++) {
        for(int k = 0; k < width_; k++) {
          int curr = bottom_label[i*height_*width_ + j*width_ + k];
          for(int c = 0; c < 4; c++) {
            int nx = k + dx4[c], ny = j + dy4[c];   // move to 8 directions
            if(nx >= 0 && nx < width_ && ny >= 0 && ny < height_) {
              int pre = bottom_label[i*height_*width_ + ny*width_ + nx];
              int index = i*height_*width_ + ny*width_ + nx; 
              
                
                vector<int>::iterator iter = find(adjacent[curr].begin(), adjacent[curr].end(), index);
                if (iter == adjacent[curr].end()) {
                  adjacent[i*height_*width_ + j*width_ + k].push_back(index);
                }
              
            }
          }
        }
      }
      all_labels.push_back(adjacent);
    }

    counter_pos_ = counter_neg_ = 0;
    Dtype total_loss = 0;
    Blob<Dtype> scale(1, 1, 1, numfeat_);
    Dtype* scale_data = scale.mutable_cpu_data();
    for(int i = 0; i < bottom[0]->num(); i++) {
      Dtype loss_pos = 0, loss_neg = 0;
      vector<vector<int> >& adjacent = all_labels[i];

      for(int n = 0; n < numpixel; n++) {
        vector<int> currsp = adjacent[n];
        int adjsz = currsp.size();
        int lf = static_cast<int>(bottom_label[i*numpixel + n]);
        if (adjsz == 0) continue;

        for (int j = 0; j < adjsz; j++) {
          caffe_sub(numfeat_, bottom_data + i*numpixel*numfeat_ + n*numfeat_,
              bottom_data + i*numpixel*numfeat_ + currsp[j]*numfeat_, scale_data);
          Dtype sim = caffe_cpu_asum(numfeat_, scale_data);
          Dtype similarity =  sigmoid2(sim), logsim;
          int ls = static_cast<int>(bottom_label[i*numpixel + currsp[j]]);
          //LOG(INFO)<<"lf:"<<lf<<" ls:"<<ls;
          if(lf == ls) {
            if(!isinf(logsim = log(similarity))) {
              loss_pos -= logsim;
         
              counter_pos_++;
            }
          }
          else if(!isinf(logsim = log(1 - similarity))) {
            loss_neg -= logsim;
            
           
            
            counter_neg_++;
          }
        }
      }
      total_loss += (loss_pos*counter_neg_ + loss_neg*counter_pos_) / (counter_pos_+ counter_neg_);
      
    }
    top[0]->mutable_cpu_data()[0] = total_loss;
    
}

template <typename Dtype>
void SimilarityLossLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
  if (propagate_down[0]) {
    const Dtype* bottom_data = transpose_bottom.cpu_data();
    const Dtype* bottom_label = bottom[1]->cpu_data();
    Dtype* bottom_diff = bottom[0]->mutable_cpu_diff();
    caffe_set(bottom[0]->count(), Dtype(0.), bottom_diff);

    Blob<Dtype> scale(1, 1, 1, numfeat_);
    Dtype* scale_data = scale.mutable_cpu_data();
    for(int i = 0; i < bottom[0]->num(); i++) {
      vector<vector<int> >& adjacent = all_labels[i];
      for(int n = 0; n < numpixel; n++) {   // for every superpixel and its neighborhood
        int lf = static_cast<int>(bottom_label[i*numpixel + n]);
        vector<int>& currsp = adjacent[n];
        int adjsz = currsp.size();
        if(adjsz == 0) continue;

        for(int j = 0; j < adjsz; j++) {
          caffe_sub(numfeat_, bottom_data + i*numpixel*numfeat_ + n*numfeat_,
            bottom_data + i*numpixel*numfeat_ + currsp[j]*numfeat_, scale_data);
          Dtype sim = caffe_cpu_asum(numfeat_, scale_data), base_diff = 0;
          Dtype similarity = Dtype(2) * sigmoid2(sim);
          int ls = static_cast<int>(bottom_label[i*numpixel + currsp[j]]);

          if((lf == ls && isinf(log(similarity))) || (lf != ls && isinf(log(1 - similarity)))
          || (lf == ls && similarity == 1) || (lf != ls && (1 - similarity) == 1))
            continue;

          base_diff = ((lf == ls) ? (Dtype(1) / (Dtype(1) + exp(-sim)) * counter_neg_)
            : (-Dtype(2) / (exp(sim) - exp(-sim)) * counter_pos_)) / (counter_neg_ + counter_pos_);
          for(int c = 0; c < numfeat_; c++) {
            Dtype diff = (scale_data[c] < 0) ? -base_diff : base_diff;
            bottom_diff[i*numpixel*numfeat_ + n*numfeat_ + c] += diff;
            bottom_diff[i*numpixel*numfeat_ + currsp[j]*numfeat_ + c] -= diff;
          }
        }
      }
    }
    // Scale down gradient
    Dtype loss_weight = top[0]->cpu_diff()[0];
    caffe_scal(bottom[0]->count(), loss_weight, bottom_diff);
  }
}

#ifdef CPU_ONLY
STUB_GPU(SimilarityLossLayer);
#endif

INSTANTIATE_CLASS(SimilarityLossLayer);
REGISTER_LAYER_CLASS(SimilarityLoss);

}  // namespace caffe
