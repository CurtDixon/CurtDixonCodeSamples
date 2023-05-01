// autohandle.h

class auto_handle
{
public:
	auto_handle()
	{
		h = NULL;
	}
	~auto_handle()
	{
		if (h)
		{
			NTSTATUS rc = ZwClose(h);
			if (rc != STATUS_SUCCESS)
				DbgPrint("ZwClose failed on handle, NTSTAUS = %x\n", rc);
		}
		h = NULL;
	}
	PHANDLE operator &()
	{
		return &h;
	}
	operator HANDLE()
	{
		return h;
	}

private:
	HANDLE h;
};


class auto_object_ref
{
public:
	auto_object_ref()
	{
		pv = NULL;
	}
	~auto_object_ref()
	{
		if (pv)
			ObDereferenceObject(pv);
		pv = NULL;
	}
	PVOID operator &()
	{
		return &pv;
	}
	operator PVOID()
	{
		return pv;
	}

private:
	PVOID pv;
};


