#include "sender.h"
#include "common/common.h"
#include "common/messages.h"

#include <boost/asio.hpp>

#include "client_app.h"

#include <fstream>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include "libavutil/log.h"
}

using boost::asio::ip::tcp;

namespace Client
{

class SenderImpl : public ISender
        , public std::enable_shared_from_this<SenderImpl>
        , public Common::ObjectCounter<SenderImpl>
{
    std::thread thread;

    boost::asio::io_service io_service;
    boost::asio::io_service::work work;

    const ISenderEventsPtr handler;

    char error_buff[512];

    enum class States
    {
        Initialize,
        Sending,
        Unloading,
        CriticalStop
    } state = States::Initialize;

    AVFormatContext* input_fmt = nullptr;

    const ClientParams params;

    enum indexes
    {
        video_idx = 0,
        audio_idx = 1,
        streams_count = 2,
    };

    AVFormatContext* output_fmts[streams_count] = {};
    int input_streams[streams_count] = {};

public:
    SenderImpl(const ClientParams& params, const ISenderEventsPtr& handler)
        : work(io_service), params(params), handler(handler)
    {}

    ~SenderImpl()
    {
        if (params.mode == ClientParams::file_mode)
            av_write_trailer(output_fmts[0]);

        avformat_close_input(&input_fmt);

        for (size_t i = 0; i < streams_count; ++i)
        {
            if (output_fmts[i] && !(output_fmts[i]->oformat->flags & AVFMT_NOFILE))
            {
                avio_closep(&output_fmts[i]->pb);
                avformat_free_context(output_fmts[i]);
            }
        }
    }

    void Initialize() override
    {
        auto self = shared_from_this();
        thread = std::thread([self](){self->MainLoop();});
    }

    void Uninitialize() override
    {
        io_service.post([this](){state = States::Unloading;});
        thread.join();
    }

    void MainLoop()
    {
        //av_log_set_level(54);
        //av_log_set_callback(av_log_ffmpeg_callback);
        av_register_all();
        avformat_network_init();

        while(true)
        {
            io_service.poll_one();

            switch(state)
            {
            case States::Initialize:
                OpenStreams();
                break;
            case States::Sending:
                Process();
                break;
            case States::Unloading:
                return;
            case States::CriticalStop:
                handler->OnSenderStopped(shared_from_this());
                state = States::Unloading;
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

    void OpenStreams()
    {
        state = States::CriticalStop;

        TRY
        {
            if (!OpenInput())
                return;

            switch (params.mode)
            {
            case ClientParams::server_mode:
                if (OpenOutput())
                    state = States::Sending;
                return;
            case ClientParams::single_mode:
                if (OpenContexts(35000, 35002))
                    state = States::Sending;
                return;
            case ClientParams::file_mode:
                if (OpenToFile())
                    state = States::Sending;
                return;
            }

        }
        CATCH_ERR("Open streams error: ");
    }

    bool OpenInput()
    {
        int ret;

        if ((ret = avformat_open_input(&input_fmt, params.url.c_str(), NULL, NULL)) < 0)
        {
            LOGE("Cannot open input file " << params.url << " retcode " << ff_error(ret));
            return false;
        }

        if ((ret = avformat_find_stream_info(input_fmt, NULL)) < 0)
        {
            LOGE("Cannot find stream information");
            return false;
        }

        input_streams[video_idx] = av_find_best_stream(input_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, 0, 0);
        if (input_streams[video_idx] < 0)
        {
            LOGE("Cannot find a video stream in the input file");
            return false;
        }

        input_streams[audio_idx] = av_find_best_stream(input_fmt, AVMEDIA_TYPE_AUDIO, -1, -1, 0, 0);
        if (input_streams[audio_idx] < 0)
        {
            LOGE("Cannot find a audio stream in the input file");
            return false;
        }

        av_dump_format(input_fmt, 0, params.url.c_str(), 0);


        return true;
    }

    bool OpenToFile()
    {
        int ret;

        std::string out_filename = "/home/vgrinko/P2Lvideo/out.mkv";

        if ((ret = avformat_alloc_output_context2(&output_fmts[0], 0, 0, out_filename.c_str())) < 0)
        {
            LOGE("Cannot open output stream" << ff_error(ret));
            return false;
        }

        AVOutputFormat * ofmt = output_fmts[0]->oformat;
        for (int i = 0; i < streams_count; i++) {
            AVStream *in_stream = input_fmt->streams[input_streams[i]];
            AVStream *out_stream = avformat_new_stream(output_fmts[0], nullptr);
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

            //if (output_fmts[0]->oformat->flags & AVFMT_GLOBALHEADER)
              //  out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            //CODEC_FLAG_GLOBAL_HEADER
        }
        av_dump_format(output_fmts[0], 0, out_filename.c_str(), 1);
        if (!(ofmt->flags & AVFMT_NOFILE)) {
            ret = avio_open(&output_fmts[0]->pb, out_filename.c_str(), AVIO_FLAG_WRITE);
            if (ret < 0) {
                fprintf(stderr, "Could not open output file '%s'", out_filename.c_str());
                return false;
            }
        }
        ret = avformat_write_header(output_fmts[0], NULL);
        if (ret < 0) {
            fprintf(stderr, "Error occurred when opening output file\n");
            return false;
        }

        av_dump_format(output_fmts[0], 0, out_filename.c_str(), 1);

        return true;
    }

    // using blocking socket I/O for simplicity
    bool OpenOutput()
    {
        boost::asio::io_service service;
        tcp::socket s(service);
        tcp::endpoint endpoint(boost::asio::ip::address::from_string(params.server_addr), params.server_port);

        s.connect(endpoint);

        char request = Common::Messages::RESERVE_TWO_PORTS;
        boost::asio::write(s, boost::asio::buffer(&request, 1));

        uint16_t reply[2];
        size_t reply_length = boost::asio::read(s, boost::asio::buffer(reply));

        uint16_t port1 = ntohs(reply[0]);
        uint16_t port2 = ntohs(reply[1]);

        LOG("Got ports from server " << port1 << ";" << port2);

        if (!OpenContexts(port1, port2))
            return false;

        char sdp[2000];
        sdp[0] = Common::Messages::START_STREAM;
        av_sdp_create(output_fmts, 2, sdp+1, 2000-1);

        LOG(sdp);

        std::ofstream sdpfile("/home/vgrinko/sdp.sdp");
        sdpfile.write(sdp+1, strlen(sdp)-1);
        sdpfile.close();

        boost::asio::write(s, boost::asio::buffer(sdp, strlen(sdp)+1));

        uint8_t sdpreply[2] = {};
        boost::asio::read(s, boost::asio::buffer(sdpreply, 2));

        LOG("Got reply on START_STREAM: " << sdpreply[0] << sdpreply[1]);
        return (sdpreply[0] == 'O' && sdpreply[1] == 'K');
    }

    bool OpenContexts(uint16_t port1, uint16_t port2)
    {
        auto rtp_ep = [this](int port){
            return "rtp://" + params.server_addr + ":" + std::to_string(port);
        };

        if (!OpenOutputContext(video_idx, rtp_ep(port1)))
            return false;

        if (!OpenOutputContext(audio_idx, rtp_ep(port2)))
            return false;

        return true;

    }

    bool OpenOutputContext(int idx, const std::string& url)
    {
        int ret;

        AVFormatContext*& ctx = output_fmts[idx];
        int stream_idx = input_streams[idx];

        AVOutputFormat *oformat = av_guess_format("rtp", NULL, NULL);

        if ((ret = avformat_alloc_output_context2(&ctx, oformat, NULL, url.c_str())) < 0)
        {
            LOGE("Cannot open output stream" << ff_error(ret));
            return false;
        }

        AVStream* in_stream = input_fmt->streams[stream_idx];
        AVStream* out_stream = avformat_new_stream(ctx, nullptr);

        ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);

        if(ret < 0)
        {
            LOGE("Error copy codec context " << ff_error(ret));
            return  false;
        }

        if (!(ctx->flags & AVFMT_NOFILE))
            avio_open(&ctx->pb, url.c_str(), AVIO_FLAG_WRITE);

        // Write output file header
         ret = avformat_write_header(ctx, NULL);
         if (ret < 0)
         {
             LOGE("Error occurred when opening output file " << ff_error(ret));
             return  false;
         }

         av_dump_format(ctx, 0, url.c_str(), 1);

        return true;
    }

    void Process()
    {
        int ret;
        AVPacket packet;

        if ((ret = av_read_frame(input_fmt, &packet)) < 0)
        {
            state = States::CriticalStop;
            LOGW("Error read frame: " << ff_error(ret));
            return;
        }

        //av_pkt_dump2(stdout, &packet, 0, input_fmt->streams[packet.stream_index]);
        //LOG("Process packet " << packet.size << " " << packet.dts << " " << packet.stream_index);

        if (params.mode == ClientParams::file_mode)
        {
            WritePacketToFile(packet);
        }
        else
        {
            SendPacketToServer(packet);
        }


        av_packet_unref(&packet);
    }

    void WritePacketToFile(AVPacket& packet)
    {
        int ret;

        AVStream* in_stream = input_fmt->streams[packet.stream_index];
        AVStream* out_stream = output_fmts[0]->streams[packet.stream_index];

        packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, out_stream->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, out_stream->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.duration = av_rescale_q(packet.duration, in_stream->time_base, out_stream->time_base);
        packet.pos = -1;

//           av_pkt_dump2(stdout, &packet, 0, input_fmt->streams[packet.stream_index]);

         if ((ret = av_interleaved_write_frame(output_fmts[0], &packet)) < 0)
        {
            state = States::CriticalStop;
            LOGW("Error write video frame: " << ff_error(ret));
        }
    }

    void SendPacketToServer(AVPacket& packet)
    {
        int ret;
        int idx = streams_count;

        if (packet.stream_index == input_streams[video_idx])
            idx = video_idx;
        else
            if (packet.stream_index == input_streams[audio_idx])
                idx = audio_idx;
        else
                return;

        AVStream* in_stream = input_fmt->streams[packet.stream_index];
        AVStream* out_stream = output_fmts[idx]->streams[0];

        packet.pts = av_rescale_q_rnd(packet.pts, in_stream->time_base, out_stream->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.dts = av_rescale_q_rnd(packet.dts, in_stream->time_base, out_stream->time_base, AVRounding(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.duration = av_rescale_q(packet.duration, in_stream->time_base, out_stream->time_base);
        packet.pos = -1;
        packet.stream_index = 0;

        //av_pkt_dump2(stdout, &packet, 0, input_fmt->streams[packet.stream_index]);

        if ((ret = av_write_frame(output_fmts[idx], &packet)) < 0 )
        {
            state = States::CriticalStop;
            LOGW("Error write frame: " << ff_error(ret));
        }

        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }



};

ISenderPtr CreateSender(const ClientParams& params, const ISenderEventsPtr& handler)
{
    return std::make_shared<SenderImpl>(params, handler);
}
}
