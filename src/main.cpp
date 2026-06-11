#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <opencv2/face.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/opencv.hpp>
#include <thread>
#include <vector>

struct FeaturesData {
  std::mutex mtx;
  std::condition_variable cv;
  cv::Mat frame;
  std::vector<cv::Rect> features;
  bool has_new_frame = false;
  bool quit = false;
  std::atomic<double> last_confidence{0.0};
};

cv::Ptr<cv::face::LBPHFaceRecognizer> model =
    cv::face::LBPHFaceRecognizer::create();
std::chrono::steady_clock::time_point last_sample_time =
    std::chrono::steady_clock::now();

bool face_detected_this_frame = false;
std::chrono::steady_clock::time_point last_seen_time;
const int DOOR_TIMEOUT_MS = 3000;

std::chrono::steady_clock::time_point last_door_tick =
    std::chrono::steady_clock::now();

std::vector<cv::Mat> training_images;
std::vector<int> training_labels;

bool flag_open_door = false;
float door_position = 0.0f;

double minimum_confidence = 70.0;

enum AppState { TRAINING_MODE, MODEL_TRAINING, RECOGNITION_MODE };

void processing_worker(FeaturesData &dataInstance,
                       cv::CascadeClassifier &face_cascade, AppState &State) {
  while (true) {
    cv::Mat grayscale_frame;
    {
      std::unique_lock<std::mutex> lock(dataInstance.mtx);
      dataInstance.cv.wait(lock, [&] {
        return dataInstance.has_new_frame || dataInstance.quit;
      });

      if (dataInstance.quit) {
        std::cout << "Exit.";
        return;
      }

      grayscale_frame = dataInstance.frame.clone();
    }
    std::vector<cv::Rect> local_features;
    face_cascade.detectMultiScale(grayscale_frame, local_features, 1.1, 4, 0,
                                  cv::Size(30, 30));
    std::vector<cv::Rect> verified_features;
    if (!local_features.empty()) {
      cv::Rect face_rect = local_features[0];
      cv::Mat face_roi = grayscale_frame(face_rect);
      cv::Mat resized_face;
      cv::resize(face_roi, resized_face, cv::Size(128, 128));
      if (State == TRAINING_MODE) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - last_sample_time)
                           .count();
        if (elapsed >= 500) {
          if (training_images.size() < 20) {
            training_images.push_back(resized_face.clone());
            training_labels.push_back(1);
            last_sample_time = now;
            if (training_images.size() >= 20) {
              State = MODEL_TRAINING;
            }
          }
        }
      } else if (State == RECOGNITION_MODE) {
        int predicted_label = -1;
        double conf = 0.0;

        model->predict(resized_face, predicted_label, conf);

        if (predicted_label == 1 && conf < 70.0) {
          verified_features.push_back(face_rect);
          flag_open_door = true;
          last_seen_time = std::chrono::steady_clock::now();
        } else {
          flag_open_door = false;
        }
        dataInstance.last_confidence.store(conf);
      }
    }
    {
      std::lock_guard<std::mutex> lock(dataInstance.mtx);
      dataInstance.features = verified_features;
    }
  }
}

void drawDoorWindow(const std::string &win_name, AppState State,
                    std::string conf) {
  cv::Mat door_canvas = cv::Mat::zeros(400, 400, CV_8UC3);

  // 1. Calculate Delta Time (seconds elapsed since last frame)
  auto current_tick = std::chrono::steady_clock::now();
  float delta_time = std::chrono::duration_cast<std::chrono::microseconds>(
                         current_tick - last_door_tick)
                         .count() /
                     1000000.0f;
  last_door_tick = current_tick; // Update time anchor for next frame

  // Clamp delta time to avoid massive animation jumps during window stutters
  if (delta_time > 0.1f)
    delta_time = 0.1f;

  if (State == TRAINING_MODE) {
    float progress = static_cast<float>(training_images.size()) / 20.0f;
    int bar_max_width = 300;
    int current_bar_width = static_cast<int>(progress * bar_max_width);

    cv::rectangle(door_canvas, cv::Point(50, 180), cv::Point(350, 220),
                  cv::Scalar(100, 100, 100), 2);

    if (current_bar_width > 0) {
      cv::rectangle(door_canvas, cv::Point(52, 182),
                    cv::Point(52 + current_bar_width, 218),
                    cv::Scalar(0, 255, 0), -1);
    }

    std::string pct_text =
        "TRAINING: " + std::to_string(static_cast<int>(progress * 100)) + "%";
    cv::putText(door_canvas, pct_text, cv::Point(50, 150),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);

  } else if (State == RECOGNITION_MODE) {
    // 2. Check for Timeout Expiry
    if (flag_open_door) {
      auto now = std::chrono::steady_clock::now();
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now - last_seen_time)
                         .count();

      if (elapsed > DOOR_TIMEOUT_MS) {
        flag_open_door = false;
      }
    }

    // 3. Apply Delta Time to Slide Progress
    // DOOR_SPEED = 1.5f means the door takes roughly 0.66 seconds to slide 100%
    // open
    const float DOOR_SPEED = 1.5f;

    if (flag_open_door) {
      if (door_position < 1.0f)
        door_position += DOOR_SPEED * delta_time;
      if (door_position > 1.0f)
        door_position = 1.0f; // Clamp maximum open boundary
    } else {
      if (door_position > 0.0f)
        door_position -= DOOR_SPEED * delta_time;
      if (door_position < 0.0f)
        door_position = 0.0f; // Clamp maximum closed boundary
    }

    // Calculate sliding panels offset based on current position
    int max_slide = 160;
    int current_slide = static_cast<int>(door_position * max_slide);

    // Draw Door Frame Border (Static Gray Background)
    cv::rectangle(door_canvas, cv::Point(30, 50), cv::Point(370, 360),
                  cv::Scalar(50, 50, 50), -1);
    cv::rectangle(door_canvas, cv::Point(40, 60), cv::Point(360, 350),
                  cv::Scalar(10, 10, 10), -1);

    // Draw Left Sliding Panel (Blue/Cyan)
    cv::rectangle(door_canvas, cv::Point(40 - current_slide, 60),
                  cv::Point(200 - current_slide, 350), cv::Scalar(230, 140, 10),
                  -1);

    // Draw Right Sliding Panel (Blue/Cyan)
    cv::rectangle(door_canvas, cv::Point(200 + current_slide, 60),
                  cv::Point(360 + current_slide, 350), cv::Scalar(230, 140, 10),
                  -1);

    // Overlay Text Status Indicators
    if (door_position > 0.9f) {
      cv::putText(door_canvas, "STATUS: OPEN", cv::Point(110, 35),
                  cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
    } else if (door_position < 0.1f) {
      cv::putText(door_canvas, "STATUS: LOCKED", cv::Point(110, 35),
                  cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 255), 2);
    } else {
      cv::putText(door_canvas, "STATUS: MOVING", cv::Point(110, 35),
                  cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 255), 2);
    }

    cv::putText(door_canvas, "CONFIDENCE: " + conf, cv::Point(30, 390),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 255), 2);
  }

  cv::imshow(win_name, door_canvas);
}

int main(int argc, char *argv[]) {
  cv::CascadeClassifier face_cascade;
  if (!face_cascade.load("haarcascade_frontalface_default.xml")) {
    std::cerr << "Couldn't load cascade filter! Are you sure it's named "
                 "'haarcascade_frontalface_default.xml'?"
              << std::endl;
    return -1;
  }

  const char *const window_name{"Automatic Door Opener"};
  const char *const door_window_name{"Electronic Shield Door"};

  cv::namedWindow(window_name, cv::WINDOW_GUI_NORMAL | cv::WINDOW_AUTOSIZE);
  cv::namedWindow(door_window_name,
                  cv::WINDOW_GUI_NORMAL | cv::WINDOW_AUTOSIZE);

  cv::VideoCapture capture(0);
  if (!capture.isOpened()) {
    std::cerr << "Cannot open video capture." << std::endl;
    return -1;
  }

  FeaturesData dataInstance;
  AppState State = TRAINING_MODE;

  std::thread worker(processing_worker, std::ref(dataInstance),
                     std::ref(face_cascade), std::ref(State));

  cv::Mat image;

  while (capture.read(image) and (!image.empty())) {
    cv::Mat grayscale_image;
    cv::cvtColor(image, grayscale_image, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(grayscale_image, grayscale_image);

    if (State == MODEL_TRAINING) {
      std::cout << "Compiling LBPH model. Please wait..." << std::endl;
      model->train(training_images, training_labels);
      std::cout << "Model Trained! Switching to Recognition Mode." << std::endl;
      State = RECOGNITION_MODE;
    }

    {
      std::lock_guard<std::mutex> lock(dataInstance.mtx);
      dataInstance.frame = grayscale_image.clone();
      dataInstance.has_new_frame = true;
    }
    dataInstance.cv.notify_one();

    std::vector<cv::Rect> features_to_draw;
    {
      std::lock_guard<std::mutex> lock(dataInstance.mtx);
      features_to_draw = dataInstance.features;
    }

    for (auto &&feature : features_to_draw) {
      cv::rectangle(image, feature, cv::Scalar(0, 255, 0), 2);
    }

    double live_conf = dataInstance.last_confidence.load();

    double short_conf = std::trunc(live_conf * 100.0) / 100.0;

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << short_conf;

    drawDoorWindow(door_window_name, State, ss.str());

    cv::imshow(window_name, image);
    int key = cv::waitKey(10);
    if (key == 'q' || key == 'Q') {
      dataInstance.quit = true;
      dataInstance.cv.notify_one();
      worker.join();
      std::_Exit(EXIT_SUCCESS);
    }
  }
  return EXIT_SUCCESS;
}
