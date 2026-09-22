// SmpChannel.h: interface for the CSmpChannel class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SMPCHANNEL_H__24DDFE69_036E_4328_B5A6_86D49B11BB6D__INCLUDED_)
#define AFX_SMPCHANNEL_H__24DDFE69_036E_4328_B5A6_86D49B11BB6D__INCLUDED_

#include <stdlib.h>
#include "BaseChan.h"
#include "SmpPackage.h"


class CSmpChannel : public CBaseChannel
{
public:
	CSmpChannel();
	virtual ~CSmpChannel();

	virtual int GetProtocolType();

protected:
	CSmpPackage m_Package;
};

#endif // !defined(AFX_SMPCHANNEL_H__24DDFE69_036E_4328_B5A6_86D49B11BB6D__INCLUDED_)
