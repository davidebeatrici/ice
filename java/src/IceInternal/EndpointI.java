// **********************************************************************
//
// Copyright (c) 2003-2014 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

package IceInternal;

abstract public class EndpointI implements Ice.Endpoint, java.lang.Comparable<EndpointI>
{
    public String toString()
    {
        return _toString();
    }

    public String _toString()
    {
        //
        // WARNING: Certain features, such as proxy validation in Glacier2,
        // depend on the format of proxy strings. Changes to toString() and
        // methods called to generate parts of the reference string could break
        // these features. Please review for all features that depend on the
        // format of proxyToString() before changing this and related code.
        //
        return protocol() + options();
    }

    //
    // Marshal the endpoint.
    //
    public abstract void streamWrite(BasicStream s);

    //
    // Return the endpoint type.
    //
    public abstract short type();

    //
    // Return the protocol name.
    //
    public abstract String protocol();

    //
    // Return the timeout for the endpoint in milliseconds. 0 means
    // non-blocking, -1 means no timeout.
    //
    public abstract int timeout();

    //
    // Return a new endpoint with a different timeout value, provided
    // that timeouts are supported by the endpoint. Otherwise the same
    // endpoint is returned.
    //
    public abstract EndpointI timeout(int t);

    //
    // Return the connection ID
    //
    public abstract String connectionId();

    //
    // Return a new endpoint with a different connection id.
    //
    public abstract EndpointI connectionId(String connectionId);

    //
    // Return true if the endpoints support bzip2 compress, or false
    // otherwise.
    //
    public abstract boolean compress();

    //
    // Return a new endpoint with a different compression value,
    // provided that compression is supported by the
    // endpoint. Otherwise the same endpoint is returned.
    //
    public abstract EndpointI compress(boolean co);

    //
    // Return true if the endpoint is datagram-based.
    //
    public abstract boolean datagram();

    //
    // Return true if the endpoint is secure.
    //
    public abstract boolean secure();

    //
    // Return a server side transceiver for this endpoint, or null if a
    // transceiver can only be created by an acceptor. In case a
    // transceiver is created, this operation also returns a new
    // "effective" endpoint, which might differ from this endpoint,
    // for example, if a dynamic port number is assigned.
    //
    public abstract Transceiver transceiver(EndpointIHolder endpoint);

    //
    // Return connectors for this endpoint, or empty list if no connector
    // is available.
    //
    public abstract java.util.List<Connector> connectors(Ice.EndpointSelectionType selType);
    public abstract void connectors_async(Ice.EndpointSelectionType selType, EndpointI_connectors callback);

    //
    // Return an acceptor for this endpoint, or null if no acceptors
    // is available. In case an acceptor is created, this operation
    // also returns a new "effective" endpoint, which might differ
    // from this endpoint, for example, if a dynamic port number is
    // assigned.
    //
    public abstract Acceptor acceptor(EndpointIHolder endpoint, String adapterName);

    //
    // Expand endpoint out in to separate endpoints for each local
    // host if listening on INADDR_ANY.
    //
    public abstract java.util.List<EndpointI> expand();

    //
    // Check whether the endpoint is equivalent to another one.
    //
    public abstract boolean equivalent(EndpointI endpoint);

    public abstract String options();

    public void initWithOptions(java.util.ArrayList<String> args)
    {
        java.util.ArrayList<String> unknown = new java.util.ArrayList<String>();

        String str = "`" + protocol() + " ";
        for(String p : args)
        {
            if(IceUtilInternal.StringUtil.findFirstOf(p, " \t\n\r") != -1)
            {
                str += " \"" + p + "\"";
            }
            else
            {
                str += " " + p;
            }
        }
        str += "'";

        for(int n = 0; n < args.size(); ++n)
        {
            String option = args.get(n);
            if(option.length() < 2 || option.charAt(0) != '-')
            {
                unknown.add(option);
                continue;
            }

            String argument = null;
            if(n + 1 < args.size() && args.get(n + 1).charAt(0) != '-')
            {
                argument = args.get(++n);
            }

            if(!checkOption(option, argument, str))
            {
                unknown.add(option);
                if(argument != null)
                {
                    unknown.add(argument);
                }
            }
        }

        args.clear();
        args.addAll(unknown);
    }

    //
    // Compare endpoints for sorting purposes.
    //
    public boolean equals(java.lang.Object obj)
    {
        if(!(obj instanceof EndpointI))
        {
            return false;
        }
        return compareTo((EndpointI)obj) == 0;
    }

    protected boolean checkOption(String option, String argument, String endpoint)
    {
        // Must be overridden to check for options.
        return false;
    }
}
