#include "init.h"
#include "httplib.h"
using namespace httplib;

int main(int argc, char *argv[])
{
    int fd;
    unsigned long ctr_data;
    char *ser_ip = NULL;

    fd = init_device(&ctr_data);

    Server svr;
    // 原有测试接口
    svr.Get("/get_bmp", [&](const Request&, Response& res) {
        res.set_header("Content-Disposition", "attachment; filename=res.bmp");
        res.set_file_content("/home/root/cis_app/res.bmp", "image/bmp");
    });

    svr.Get("/get_b_bmp", [&](const Request&, Response& res) {
        res.set_header("Content-Disposition", "attachment; filename=res_b.bmp");
        res.set_file_content("/home/root/cis_app/res_b.bmp", "image/bmp");
        });

    svr.Get("/get_origin_bmp", [&](const Request&, Response& res) {
        res.set_header("Content-Disposition", "attachment; filename=origin.bmp");
        res.set_file_content("/home/root/cis_app/origin.bmp", "image/bmp");
    });
    //获取b的图像
    svr.Get("/get_origin_b_bmp", [&](const Request&, Response& res) {
        res.set_header("Content-Disposition", "attachment; filename=origin_b.bmp");
        res.set_file_content("/home/root/cis_app/origin_b.bmp", "image/bmp");
        });

    svr.Get("/get_white_bmp", [&](const Request&, Response& res) {
        res.set_header("Content-Disposition", "attachment; filename=white.bmp");
        res.set_file_content("/home/root/cis_app/white.bmp", "image/bmp");
    });
    //获取b的白照片
    svr.Get("/get_white_b_bmp", [&](const Request&, Response& res) {
        res.set_header("Content-Disposition", "attachment; filename=white_b.bmp");
        res.set_file_content("/home/root/cis_app/white_b.bmp", "image/bmp");
        });

    svr.Get("/get_black_bmp", [&](const Request&, Response& res) {
        res.set_header("Content-Disposition", "attachment; filename=black.bmp");
        res.set_file_content("/home/root/cis_app/black.bmp", "image/bmp");
    });
    //获取b的黑照片
    svr.Get("/get_black_b_bmp", [&](const Request&, Response& res) {
        res.set_header("Content-Disposition", "attachment; filename=black_b.bmp");
        res.set_file_content("/home/root/cis_app/black_b.bmp", "image/bmp");
        });

    svr.Get("/start_scan", [&](const Request&, Response& res) {
        start_scan(fd);
        res.set_content("Success scan!", "text/plain");
    });

    svr.Get("/chose_the_same", [&](const Request&, Response& res) {
        update_bmpFile("/home/root/cis_app/origin.bmp",0);
        res.set_content("chose_the_same!", "text/plain");
        });
    svr.Get("/get_img", [&](const Request&, Response& res) {
        get_img(fd, 0,0,0);
        res.set_content("A new image was successfully acquired!", "text/plain");
    });
    //只给a的//////////////////////////////////
    svr.Get("/get_white_a_img", [&](const Request&, Response& res) {
        get_img(fd, 1,0,0);
        res.set_content("A new white image for a was successfully acquired!", "text/plain");
    });
    svr.Get("/get_black_a_img", [&](const Request&, Response& res) {
        get_img(fd, 0, 1,0);
        res.set_content("A new black image for a was successfully acquired!", "text/plain");
    });
    //只给b的//////////////////////////////////
    svr.Get("/get_white_b_img", [&](const Request&, Response& res) {
        get_img(fd, 1, 0,1);
        res.set_content("A new white image  for b was successfully acquired!", "text/plain");
        });
    svr.Get("/get_black_b_img", [&](const Request&, Response& res) {
        get_img(fd, 0, 1,1);
        res.set_content("A new black image for b was successfully acquired!", "text/plain");
        });
    //a的和b的同时搞//////////////////////////////////
    svr.Get("/get_white_both_img", [&](const Request&, Response& res) {
        get_img(fd, 1, 0,2);
        res.set_content("A new white image for two was successfully acquired!", "text/plain");
        });
    svr.Get("/get_black_both_img", [&](const Request&, Response& res) {
        get_img(fd, 0, 1,2);
        res.set_content("A new black image for two was successfully acquired!", "text/plain");
        });
    //分界线/////////////////////////////////////////////

    svr.Get("/update_weight", [&](const Request&, Response& res) {
        update_weight_all();
        res.set_content("Weight updated!", "text/plain");
    });
    //加入选择扫描仪的
    svr.Get("/chose_a", [&](const Request&, Response& res) {
        chose_scanner(fd, 0);
        change_scanner(0);
        //update_weight(); //把选择照片变成a,同时更新校正的参数
        res.set_content("chose a!", "text/plain");
    });
    svr.Get("/chose_b", [&](const Request&, Response& res) {
        chose_scanner(fd, 1);
        change_scanner(1);
        //update_weight(); //把照片选择变成b，同时更新校正参数
        res.set_content("chose b!", "text/plain");
    });

    svr.Get("/chose_change_1", [&](const Request&, Response& res) {
        chose_scanner(fd, 2);
       
        //update_weight(); //把选择照片变成a,同时更新校正的参数
        res.set_content("change==1!", "text/plain");
        });
    svr.Get("/chose_change_0", [&](const Request&, Response& res) {
        chose_scanner(fd, 3);

        //update_weight(); //把选择照片变成a,同时更新校正的参数
        res.set_content("change==0!", "text/plain");
        });

    //分辨率设置
    svr.Get("/dpi_300", [&](const Request&, Response& res) {
        dpi_chose(fd, 0);
        res.set_content("dpi set for 300!", "text/plain");
        });
    svr.Get("/dpi_600", [&](const Request&, Response& res) {
        dpi_chose(fd, 1);
        res.set_content("dpi set for 600!", "text/plain");
        });
    svr.Get("/dpi_1200", [&](const Request&, Response& res) {
        dpi_chose(fd, 2);
        res.set_content("dpi set for 1200!", "text/plain");
        });

    svr.Get("/get_black_offset", [&](const Request&, Response& res) {
        BlackOffsetValues values = read_black_offset(fd);

        // 将结果转换为字符串并设置为响应内容
        std::ostringstream oss;
        oss << "Black Offset Values: " << static_cast<int>(values.high8) << ", "
            << static_cast<int>(values.mid8) << ", "
            << static_cast<int>(values.low8);
        res.set_content(oss.str(), "text/plain");
        });
    
    svr.Get("/img_flip", [&](const Request&, Response& res) {
        img_flip();
        res.set_content("The setup was successful!", "text/plain");
    });

    svr.Get("/get_data", [&](const Request& req, Response& res) {
    // 1. 获取参数
    if (!req.has_param("number")) {
        res.status = 400; // Bad Request
        res.set_content("Missing 'number' parameter", "text/plain");
        return;
    }
    // 2. 转换参数为整数
    std::string num_str = req.get_param_value("number");
    try {
        int number = std::stoi(num_str);
        set_interval_time(fd,number);
        res.set_content("Received number: " + std::to_string(number), "text/plain");
    } catch (const std::exception&) {
        res.status = 400; // Bad Request
        res.set_content("Invalid number format", "text/plain");
    }
});
    svr.listen("0.0.0.0", 5555);

  close(fd);
  return 0;
}
