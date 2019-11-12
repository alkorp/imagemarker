#include "image_processor.hpp"

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"

const auto kEncodeFormat = ".png";

bool StampText(const std::string &txt, std::vector<unsigned char> &image) {
  cv::Mat img = cv::imdecode(image, -1);
  if (!img.data)
    return false;

  std::string text(txt.begin(), txt.end());
  const int font_face = cv::FONT_HERSHEY_SIMPLEX;
  const double font_scale = 1;
  const int thickness = 1;

  int baseline = 0;
  cv::Size text_size =
      cv::getTextSize(text, font_face, font_scale, thickness, &baseline);

  cv::Point text_org(0, text_size.height);
  cv::putText(img, text, text_org, font_face, font_scale, cv::Scalar::all(0),
              thickness, 8);
  return cv::imencode(kEncodeFormat, img, image);
}
