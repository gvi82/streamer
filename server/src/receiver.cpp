#include "receiver.h"
#include "common/common.h"

#include <thread>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include "libavutil/log.h"
}

namespace Server
{

class timeout_handler {
    using clock = std::chrono::steady_clock;
    using ms = std::chrono::milliseconds;
    clock::time_point start;
    unsigned last_delay = 0;
    int reset_line;
    static constexpr unsigned timeout_ms = 5000;
  public:
    timeout_handler()
    {
        reset(__LINE__);
    }

    void reset(int line)
    {
        start = clock::now();
        last_delay = 0;
        reset_line = line;
    }

    int is_timeout()
    {
        auto actualDelay = std::chrono::duration_cast<ms>(clock::now() - start).count();
        if ((last_delay/1000) != (actualDelay/1000))
        {
            last_delay = actualDelay;
            if (last_delay > 3000)
            {
                //LOGW("delay deceteced: " << (last_delay/1000) << " (+" << reset_line << ")");
            }
        }
        return actualDelay > timeout_ms ? 1 : 0;
    }

    static int check_interrupt(void * t)
    {
        timeout_handler* th = static_cast<timeout_handler *>(t);
        if (th && th->is_timeout())
        {
            LOGW("Interrupt listen by timout " << th->reset_line);
            return 1;
        }

       return 0;
    }
 };


class Receiver : public IReceiver
        , public Common::ObjectCounter<Receiver>
        , public std::enable_shared_from_this<Receiver>
{
    std::thread thread;
    bool runing = true;
    const std::string sdp;
    const IReceiverCallbackPtr callback;

    AVFormatContext* input_fmt = nullptr;
    AVFormatContext* output_fmt = nullptr;

    timeout_handler th;

    char error_buff[512];

    const int video_id;

    enum class States
    {
        OpenInput,
        OpenOutput,
        Process,
        Fail,
        Unloading,
    } state = States::OpenInput;

public:

    Receiver(const IReceiverCallbackPtr& callback, const std::string& sdp, int video_id) : sdp(sdp), callback(callback), video_id(video_id)
    {
        LOG("Receiver CONSTRUCT " << this);
    }

    ~Receiver()
    {
        LOG("Receiver DESTROY " << this);
        if (output_fmt)
            av_write_trailer(output_fmt);
        avformat_close_input(&input_fmt);
        if (output_fmt && !(output_fmt->oformat->flags & AVFMT_NOFILE))
            avio_closep(&output_fmt->pb);
        avformat_free_context(output_fmt);
    }

    void Initialize() override
    {
        auto self(shared_from_this());
        thread = std::thread([self]()
        {
            self->RunAccept();
        });
    }

    void Uninitialize() override
    {
        runing = false;
        thread.join();
        LOG("Uninitialize successed");
    }

    void RunAccept()
    {
        av_log_set_level(54);
        //av_log_set_callback(av_log_ffmpeg_callback);
        av_register_all();
        avformat_network_init();

        while(runing)
        {
            switch (state)
            {
            case States::OpenInput:
                OpenInput();
                break;
            case States::OpenOutput:
                OpenOutput();
                break;
            case States::Process:
                Process();
                break;
            case States::Fail:
                callback->OnReceiverFailed();
                state = States::Unloading;
                break;
            case States::Unloading:
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                break;
            }
        }
    }

    const char* ff_error(int errcode)
    {
        error_buff[0] = 0;
        av_strerror(errcode, error_buff, sizeof(error_buff));
        return error_buff;
    }

    void OpenInput()
    {
        state = States::Fail;
        int ret;

        AVDictionary *dict = NULL;
        av_dict_set(&dict, "protocol_whitelist", "file,udp,rtp", 0);

        // echo 2097152 > /proc/sys/net/core/rmem_max
        av_dict_set(&dict, "buffer_size", "10000000", 0);
        av_dict_set(&dict, "fifo_size", "100000", 0);

        if ((ret = avformat_open_input(&input_fmt, sdp.c_str(), NULL, &dict)) < 0)
        {
            LOGE("Cannot open input sdp stream " << ff_error(ret));
            return;
        }

        input_fmt->interrupt_callback.opaque = &th;
        input_fmt->interrupt_callback.callback = &timeout_handler::check_interrupt;

        if (dict)
        {
            av_dict_free(&dict);
            LOGW("Not all options passed");
        }

        callback->OnReceiverStarted();

        if ((ret = avformat_find_stream_info(input_fmt, NULL)) < 0)
        {
            LOGE("Cannot find stream information");
            return;
        }

        av_dump_format(input_fmt, 0, sdp.c_str(), 0);

        state = States::OpenOutput;
    }

    bool OpenOutput()
    {
        state = States::Fail;

        int ret;

        std::ostringstream fstr;
        fstr << "out" << std::to_string(video_id) << ".mp4";

        std::string out_filename = fstr.str();

        if ((ret = avformat_alloc_output_context2(&output_fmt, 0, 0, out_filename.c_str())) < 0)
        {
            LOGE("Cannot open output stream" << ff_error(ret));
            return false;
        }

        AVOutputFormat * ofmt = output_fmt->oformat;
        for (int i = 0; i < input_fmt->nb_streams; i++) {
            AVStream *in_stream = input_fmt->streams[i];
            AVStream *out_stream = avformat_new_stream(output_fmt, nullptr);

            if (!out_stream) {
                fprintf(stderr, "Failed allocating output stream\n");
                ret = AVERROR_UNKNOWN;
                return false;
            }
            ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            if (ret < 0) {
                fprintf(stderr, "Failed to copy context from input to output stream codec context\n");
                return false;
            }
            //if (output_fmt->oformat->flags & AVFMT_GLOBALHEADER)
              //  out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }

        if (!(ofmt->flags & AVFMT_NOFILE)) {
            ret = avio_open(&output_fmt->pb, out_filename.c_str(), AVIO_FLAG_WRITE);
            if (ret < 0) {
                fprintf(stderr, "Could not open output file '%s'", out_filename.c_str());
                return false;
            }
        }
        ret = avformat_write_header(output_fmt, NULL);
        if (ret < 0) {
            fprintf(stderr, "Error occurred when opening output file\n");
            return false;
        }

        av_dump_format(output_fmt, 0, out_filename.c_str(), 1);

        state = States::Process;

        return true;
    }

    unsigned total = 0;
    void Process()
    {
        int ret;
        AVPacket pkt;
        //LOG("Process");

        if ((ret = av_read_frame(input_fmt, &pkt)) < 0)
        {
            state = States::Fail;
            LOGW("Error read frame: " << ff_error(ret));
            return;
        }

        th.reset(__LINE__);

        //LOG("Got packet " << total << " stream " << pkt.stream_index << " " << pkt.dts);

        AVStream* in_stream = input_fmt->streams[pkt.stream_index];
        AVStream* out_stream = output_fmt->streams[pkt.stream_index];


        total += pkt.size;
        //av_pkt_dump2(stdout, &pkt, 0, input_fmt->streams[pkt.stream_index]);

        if ((ret = av_write_frame(output_fmt, &pkt)) < 0)
        {
            LOGW("Error write packet: " << ff_error(ret) << " " << pkt.stream_index);
            av_pkt_dump2(stdout, &pkt, 1, input_fmt->streams[pkt.stream_index]);
            return;
        }

        av_packet_unref(&pkt);
    }
};

IReceiverPtr CreateReceiver(const IReceiverCallbackPtr& callback, const std::string& sdp, int video_id)
{
    return std::make_shared<Receiver>(callback, sdp, video_id);
}

}

