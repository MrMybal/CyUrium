# SPDX-FileCopyrightText: 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Python SDK entry point, also used by the packaged official MCP bridge."""
import asyncio
import json
import urllib.request
import http.client
from urllib.parse import urlsplit
from pathlib import Path
from connection import load
from cytools_core.transports.http import HTTPClient
from cytools_core import CyToolError


class CyUriumClient(HTTPClient):
    def __init__(self, profile):
        discovery=load(Path(profile)/'sdk/connection.bin')
        request=urllib.request.Request(discovery['url'].replace('/v1','/connect'),data=b'{}',
            headers={'Authorization':'Bearer '+discovery['secret'],'Content-Type':'application/json'})
        class NoRedirect(urllib.request.HTTPRedirectHandler):
            def redirect_request(self,*args,**kwargs): return None
        # Never send local SDK credentials via the user's proxy configuration.
        with urllib.request.build_opener(urllib.request.ProxyHandler({}),NoRedirect()).open(request,timeout=5) as response:
            credentials=json.load(response)
        super().__init__(discovery['url'],credentials['token'],timeout=30)
        self.client_id=credentials['clientId']

    def call(self, method, **params):
        # Direct socket: proxy environment variables cannot see local credentials.
        uri=urlsplit(self.url)
        connection=http.client.HTTPConnection('127.0.0.1',uri.port,timeout=self.timeout)
        try:
            request={'protocolVersion':'1.0','id':1,'method':method,'params':params}
            connection.request('POST','/v1',body=json.dumps(request,allow_nan=False).encode('utf-8'),
                headers={'Authorization':'Bearer '+self.token,'Content-Type':'application/json'})
            response=connection.getresponse()
            if response.status!=200:raise CyToolError('TransportError','Local SDK HTTP '+str(response.status))
            body=response.read(4*1024*1024+1)
            if len(body)>4*1024*1024:raise CyToolError('TransportError','SDK response too large')
            result=json.loads(body)
            if 'error' in result:
                error=result['error'];raise CyToolError(error['code'],error['message'])
            return result['result']
        finally:connection.close()


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser()
    parser.add_argument('--profile',type=Path,required=True)
    args=parser.parse_args()
    # urllib's fresh opener in the upstream client respects no_proxy.
    import os
    os.environ['no_proxy']='127.0.0.1,localhost,::1'
    from cytools_core.transports.mcp import serve
    asyncio.run(serve(CyUriumClient(args.profile)))
