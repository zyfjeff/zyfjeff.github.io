#include <iostream>

#include "sigslot.h"

using namespace sigslot;

class Sender{
 public :
  sigslot::signal2<const std::string&, int> signalSender;
	void help1(){
		signalSender("Help!",1);
	}

  void help2(){
		signalSender("Help,hahha,just test",2);
	}
};

class Receiver : public sigslot::has_slots<>{
 public:
  void onReceiver(const std::string& message,int type){
    if(type==1){
			std::cout<<"correct slot >> " << message << std::endl;
		}else{
			std::cout<<"wrong slot >> " << message << std::endl;
		}
	}
};

int main(int argc, char** argv) {
	Sender sender;
	Receiver rec;
	sender.signalSender.connect(&rec, &Receiver::onReceiver);
	sender.help1();
	sender.help2();
	sender.signalSender.disconnect(&rec);
	return 0;
}

