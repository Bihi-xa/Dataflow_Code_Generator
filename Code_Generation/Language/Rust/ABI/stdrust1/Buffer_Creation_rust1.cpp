#include "Code_Generation/Code_Generation.hpp"
#include "ABI_stdrust1.hpp"
#include "Config/config.h"
#include "Config/debug.h"
#include <string>
#include <fstream>
#include <filesystem>

/* Generate the file Channel.hpp containing the Channel base class, the data channel derived class
 * and if required the control channel derived class that is not carring data explicitly.
 */
std::string mpsc_channel =
"use tokio::sync::mpsc::{self, Receiver, Sender};\n"
"\n"
"#[derive(Clone)]\n"
"pub struct ChannelSender<T> {\n"
"    sender: Sender<T>,\n"
"    max_size: usize,\n"
"}\n"
"\n"
"pub struct ChannelReceiver<T> {\n"
"    receiver: Receiver<T>,\n"
"    max_size: usize,\n"
"}\n"
"\n"
"pub fn new_channel<T>(max_size: usize) -> (ChannelSender<T>, ChannelReceiver<T>) {\n"
"    let (sender, receiver) = mpsc::channel(max_size);\n"
"    (\n"
"        ChannelSender { sender, max_size,  },\n"
"        ChannelReceiver { receiver, max_size,},\n"
"    )\n"
"}\n"
"\n"
"impl<T> ChannelSender<T> {\n"
"    pub async fn write(&self, value: T) {\n"
"        if let Err(_) = self.sender.send(value).await {\n"
"            println!(\"Failed to send data to channel!\");\n"
"        }\n"
"    }\n"
"\n"
"    pub fn free(&self) -> usize {\n"
"        self.sender.capacity()\n"
"    }\n"
"}\n"
"\n"
"impl<T> ChannelReceiver<T> {\n"
"    pub async fn read(&mut self) -> Option<T> {\n"
"        let value = self.receiver.recv().await;\n"
"        value\n"
"    }\n"
"\n"
"    pub fn try_read(&mut self) -> Option<T> {\n"
"        self.receiver.try_recv().ok()\n"
"    }\n"
"    pub fn size(&self) -> usize {\n"
"        self.receiver.len()\n"
"    }\n"
"\n"
"    pub fn max_size(&self) -> usize {\n"
"        self.max_size\n"
"    }\n"
"\n"
"    pub fn is_terminated(&self) -> bool {\n"
"        self.receiver.is_closed()\n"
"    }\n"
"\n"
"    pub fn is_empty(&self) -> bool {\n"
"        self.receiver.len() == 0\n"
"    }\n"
"}\n";

std::pair<ABI_stdrust1::Header, ABI_stdrust1::Source>
ABI_stdrust1::generate_channel_code(bool cntrl_chan)
{
    Config* c = c->getInstance();

    std::filesystem::path path{ c->get_target_dir() };
    path /= "src";
    path /= "channel.rs";

    std::ofstream output_file{ path };
    if (output_file.fail())
    {
        throw Code_Generation::Code_Generation_Exception{ "Cannot open the file " + path.string() };
    }
    output_file << mpsc_channel;

    output_file.close();

    return std::make_pair("channel.rs", "");
}

std::pair<std::string, ABI_stdrust1::Impl_Type> ABI_stdrust1::channel_decl(
    std::string channel_name,
    std::string size,
    std::string type,
    bool static_def,
    std::string prefix)
{
    // p2: channel type
    std::string name = (channel_name == "in") ? "input" : channel_name;
    std::string p2, p1;
    p2 = "ChannelSender<" + type + ">";
    p1 = prefix;
    p1.append(name + ": " + p2 + ", \n");
    return std::make_pair(p1, p2);
}

std::pair<std::string, ABI_stdrust1::Impl_Type> ABI_stdrust1::channel_decl_sen(
    std::string channel_name,
    std::string size,
    std::string type,
    bool static_def,
    std::string prefix)
{
    std::string name = (channel_name == "in") ? "input" : channel_name;
    // p2: channel type
    std::string p2, p1;
    p2 = "ChannelSender<" + type + ">";
    p1 = prefix;
    p1.append(name + ": Option<" + p2 + ">, \n");
    return std::make_pair(p1, p2);
}

std::pair<std::string, ABI_stdrust1::Impl_Type> ABI_stdrust1::channel_decl_rec(
    std::string channel_name,
    std::string size,
    std::string type,
    bool static_def,
    std::string prefix)
{
    std::string name = (channel_name == "in") ? "input" : channel_name;
    // p2: channel type
    std::string p2, p1;
    p2 = "ChannelReceiver<" + type + ">";
    p1 = prefix;
    p1.append(name + ": " + p2 + ", \n");
    return std::make_pair(p1, p2);
}
// it needs 2 arguments instead of channel_name ; (sender,reciever)
std::string ABI_stdrust1::channel_init(
    std::string channel_sender,
    std::string channel_receiver,
    Impl_Type t,
    std::string type,
    std::string sz,
    std::string prefix)
{
    // std::string r = prefix + channel_name + " = new " + t + "(" + sz + "); \n";
    std::string r = prefix + "let ( " + channel_sender + ", " + channel_receiver + " ) = new_channel::<" + type + ">(" + sz + "); ";
    return r;
}

std::string ABI_stdrust1::channel_read(
    std::string channel)
{
    std::string name = (channel == "in") ? "input" : channel;

    return "self." + name + ".read().await";
}

std::string ABI_stdrust1::channel_write(
    std::string var,
    std::string channel,
    std::string prefix)
{
    std::string result;
    result.append(prefix + "if let Some(sender) = &self." + channel + " {\n");
    result.append(prefix + "\tsender.write((" + var + ").try_into().unwrap()).await;\n");
    result.append(prefix + "}\n");
    // return channel + ".write(" + var + ").await";
    return result;
}
// no use atm
std::string ABI_stdrust1::channel_prefetch(
    std::string channel,
    std::string offset)
{
    std::string name = (channel == "in") ? "input" : channel;
    // return channel + "->preview(" + offset + ")";
    return "self." + name + ".try_read()";
}

std::string ABI_stdrust1::channel_size(
    std::string channel)
{
    std::string name = (channel == "in") ? "input" : channel;
    return "self." + name + ".size()";
}

std::string ABI_stdrust1::channel_free(
    std::string channel)
{
    std::string output;
    std::string name = (channel == "in") ? "input" : channel;
    output.append("if let Some(ref sender) = self." + name + " {\n");
    output.append("\tif sender.free()");
    return output;
    // return "self." + channel + ".free()";
}