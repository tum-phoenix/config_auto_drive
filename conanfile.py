from conans import ConanFile, CMake

class AutoDriveConan(ConanFile):
    name = "imaging"
    version = "1.0"
    settings = None
    exports = "README.md","start.sh"
    requires = "lms/2.0@lms/stable","cereal/1.2-0@lms/stable","gtest/1.8.0@lms/stable" #"lms_camera_importer/1.0@lms/stable","lms_sdl_service/1.0@lms/stable","lms_sdl_image_renderer/1.0@lms/stable","lms_urg_laser_scanner/1.0@lms/stable","lms_image_converter/1.0@lms/stable","phoenix_service/1.0@lms/stable","image_hint_generator/1.0@lms/stable","image_hint_worker/1.0@lms/stable","image_hint_transformer/1.0@lms/stable","image_hint_worker/1.0@lms/stable","lap_time/1.0@lms/stable","socket_datatype_definer/1.0@lms/stable","lms_ximea_importer/1.0@lms/stable","lms_pause_runtime/1.0@lms/stable"#,"local_course_service/1.0@lms/stable"

    def imports(self):
        self.copy("*.so",dst=".")
        self.copy("include/*",dst="")
