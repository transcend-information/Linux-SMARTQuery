#include "nvme_util.h"
#include <fcntl.h>
#include <QMessageBox>
#include "linux_nvme_ioctl.h"
#include <sys/ioctl.h>

#define O_RDONLY	     00
#ifndef O_NONBLOCK
# define O_NONBLOCK	  04000
#endif



nvme_Device::nvme_Device()
{

}
nvme_Device::nvme_Device(const char * dev_name, const char * req_type, unsigned nsid)
{
    nvmeopts.drive_info = true;
    nvmeopts.smart_check_status = true;

    m_nsid = nsid;
    m_info.dev_name = dev_name;
    m_info.info_name = dev_name;
    m_info.dev_type = "nvme";
    m_info.req_type = req_type;
    m_fd = -1;
    m_flags = O_RDONLY | O_NONBLOCK;
    m_retry_flags = -1;
}

int nvme_Device::myOpen()
{
  m_fd = ::open(m_info.dev_name.c_str(), m_flags);

  if (m_fd < 0 /*&& errno == EROFS*/ && m_retry_flags != -1)
    // Retry
    m_fd = ::open(m_info.dev_name.c_str(), m_retry_flags);

  if (m_fd < 0) {
      return m_fd;
  }

  if (m_fd >= 0) {
    // sets FD_CLOEXEC on the opened device file descriptor.  The
    // descriptor is otherwise leaked to other applications (mail
    // sender) which may be considered a security risk and may result
    // in AVC messages on SELinux-enabled systems.
    if (-1 == fcntl(m_fd, F_SETFD, FD_CLOEXEC))
      // TODO: Provide an error printing routine in class smart_interface
        QMessageBox::information(NULL, "Error", "fcntl(set  FD_CLOEXEC) failed", NULL);
      //pout("fcntl(set  FD_CLOEXEC) failed, errno=%d [%s]\n", errno, strerror(errno));
  }

  if (!m_nsid) {
    // Use actual NSID (/dev/nvmeXnN) if available,
    // else use broadcast namespace (/dev/nvmeX)
    int nsid = ioctl(m_fd, NVME_IOCTL_ID, (void*)0);
    m_nsid = nsid;
  }

  return m_fd;
}

void nvme_Device::swap2(char *location){
  char tmp=*location;
  *location=*(location+1);
  *(location+1)=tmp;
  return;
}

// swap four bytes.  Point to low address
void nvme_Device::swap4(char *location){
  char tmp=*location;
  *location=*(location+3);
  *(location+3)=tmp;
  swap2(location+1);
  return;
}

// swap eight bytes.  Points to low address
void nvme_Device::swap8(char *location){
  char tmp=*location;
  *location=*(location+7);
  *(location+7)=tmp;
  tmp=*(location+1);
  *(location+1)=*(location+6);
  *(location+6)=tmp;
  swap4(location+2);
  return;
}

bool nvme_Device::nvme_read_id_ctrl(nvme_id_ctrl & id_ctrl)
{
  if (!nvme_read_identify(0, 0x01, &id_ctrl, sizeof(id_ctrl)))
    return false;

  if (isbigendian()) {
    swapx(&id_ctrl.vid);
    swapx(&id_ctrl.ssvid);
    swapx(&id_ctrl.cntlid);
    swapx(&id_ctrl.oacs);
    swapx(&id_ctrl.wctemp);
    swapx(&id_ctrl.cctemp);
    swapx(&id_ctrl.mtfa);
    swapx(&id_ctrl.hmpre);
    swapx(&id_ctrl.hmmin);
    swapx(&id_ctrl.rpmbs);
    swapx(&id_ctrl.nn);
    swapx(&id_ctrl.oncs);
    swapx(&id_ctrl.fuses);
    swapx(&id_ctrl.awun);
    swapx(&id_ctrl.awupf);
    swapx(&id_ctrl.acwu);
    swapx(&id_ctrl.sgls);
    for (int i = 0; i < 32; i++) {
      swapx(&id_ctrl.psd[i].max_power);
      swapx(&id_ctrl.psd[i].entry_lat);
      swapx(&id_ctrl.psd[i].exit_lat);
      swapx(&id_ctrl.psd[i].idle_power);
      swapx(&id_ctrl.psd[i].active_power);
    }
  }

  return true;
}

bool nvme_Device::nvme_read_smart_log(nvme_smart_log & smart_log)
{
  if (!nvme_read_log_page(0x02, &smart_log, sizeof(smart_log)))
    return false;

  if (isbigendian()) {
    swapx(&smart_log.warning_temp_time);
    swapx(&smart_log.critical_comp_time);
    for (int i = 0; i < 8; i++)
      swapx(&smart_log.temp_sensor[i]);
  }

  return true;
}


bool nvme_Device::nvme_read_id_ns(unsigned nsid, nvme_id_ns & id_ns)
{
  if (!nvme_read_identify(nsid, 0x00, &id_ns, sizeof(id_ns)))
    return false;

  if (isbigendian()) {
    swapx(&id_ns.nsze);
    swapx(&id_ns.ncap);
    swapx(&id_ns.nuse);
    swapx(&id_ns.nawun);
    swapx(&id_ns.nawupf);
    swapx(&id_ns.nacwu);
    swapx(&id_ns.nabsn);
    swapx(&id_ns.nabo);
    swapx(&id_ns.nabspf);
    for (int i = 0; i < 16; i++)
      swapx(&id_ns.lbaf[i].ms);
  }

  return true;
}

bool nvme_Device::nvme_pass_through(const nvme_cmd_in & in)
{
    nvme_cmd_out out;

    nvme_passthru_cmd pt;
    memset(&pt, 0, sizeof(pt));

    pt.opcode = in.opcode;
    pt.nsid = in.nsid;
    pt.addr = (uint64_t)in.buffer;
    pt.data_len = in.size;
    pt.cdw10 = in.cdw10;
    pt.cdw11 = in.cdw11;
    pt.cdw12 = in.cdw12;
    pt.cdw13 = in.cdw13;
    pt.cdw14 = in.cdw14;
    pt.cdw15 = in.cdw15;

    int status = ioctl(m_fd, NVME_IOCTL_ADMIN_CMD, &pt);

    if (status < 0)
        return false;

    if (status > 0)
        return false;

    out.result = pt.result;
    bool ok = true;

    return ok;

}


bool nvme_Device::nvme_read_identify(unsigned nsid,
  unsigned char cns, void * data, unsigned size)
{
  memset(data, 0, size);
  nvme_cmd_in in;
  in.set_data_in(nvme_admin_identify, data, size);
  in.nsid = nsid;
  in.cdw10 = cns;

  return nvme_pass_through(in);

}
bool nvme_Device::nvme_read_log_page(unsigned char lid, void * data, unsigned size)
{
  if (!(4 <= size && size <= 0x4000 && (size % 4) == 0))
  {
    QMessageBox::information(NULL, "Error", "invalid opcode", NULL);
    return false;
  }

  memset(data, 0, size);
  nvme_cmd_in in;
  in.set_data_in(nvme_admin_get_log_page, data, size);
  in.nsid = m_nsid;
  in.cdw10 = lid | (((size / 4) - 1) << 16);

  return nvme_pass_through(in);

}
