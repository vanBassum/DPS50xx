#pragma once

enum class ModbusError
{
    NoError = 0,
    IllegalFunction = 1,
    IllegalDataAddress = 2,
    IllegalDataValue = 3,
    SlaveDeviceFailure = 4,
    InvalidArguments = 5,
    InvalidReplyLength = 6,
    UnknownException = 7,
    InvalidReplyFunctionCode = 8,
    Timeout = 9,
    MutexFailed = 10,
    Disconnected = 11,
    OutOfMemory = 12,
    InsufficientRxBufferSize = 13,
    NotImplemented = 14,
    InvalidCRC = 15,
    ReadingNotAllowed = 17,
    WritingNotAllowed = 18,
    NotInitialized = 19,
    Acknowledge = 20,
    SlaveDeviceBusy = 21,
    MemoryParityError = 22,
    GatewayPathUnavailable = 23,
    GatewayTargetFailed = 24,
    InvalidDeviceReply_EchoMismatch = 25,
    InvalidDeviceReply_ProtocolId = 26,
    InvalidDeviceReply_UnitId = 27,
    InvalidDeviceReply_ReplyLength = 28,
};

constexpr const char* ModbusErrorToString(ModbusError error)
{
    switch (error)
    {
    case ModbusError::NoError: return "No Error";
    case ModbusError::IllegalFunction: return "Illegal Function";
    case ModbusError::IllegalDataAddress: return "Illegal Data Address";
    case ModbusError::IllegalDataValue: return "Illegal Data Value";
    case ModbusError::SlaveDeviceFailure: return "Slave Device Failure";
    case ModbusError::InvalidArguments: return "Invalid Arguments";
    case ModbusError::InvalidReplyLength: return "Invalid Reply Length";
    case ModbusError::UnknownException: return "Unknown Exception";
    case ModbusError::InvalidReplyFunctionCode: return "Invalid Reply Function Code";
    case ModbusError::Timeout: return "Timeout";
    case ModbusError::MutexFailed: return "Mutex Failed";
    case ModbusError::Disconnected: return "Disconnected";
    case ModbusError::OutOfMemory: return "Out of Memory";
    case ModbusError::InsufficientRxBufferSize: return "Insufficient RX Buffer Size";
    case ModbusError::NotImplemented: return "Not Implemented";
    case ModbusError::InvalidCRC: return "Invalid CRC";
    case ModbusError::ReadingNotAllowed: return "Reading Not Allowed";
    case ModbusError::WritingNotAllowed: return "Writing Not Allowed";
    case ModbusError::NotInitialized: return "Not Initialized";
    case ModbusError::Acknowledge: return "Acknowledge";
    case ModbusError::SlaveDeviceBusy: return "Slave Device Busy";
    case ModbusError::MemoryParityError: return "Memory Parity Error";
    case ModbusError::GatewayPathUnavailable: return "Gateway Path Unavailable";
    case ModbusError::GatewayTargetFailed: return "Gateway Target Failed";
    case ModbusError::InvalidDeviceReply_EchoMismatch: return "Invalid Device Reply - Echo Mismatch";
    case ModbusError::InvalidDeviceReply_ProtocolId: return "Invalid Device Reply - Protocol ID";
    case ModbusError::InvalidDeviceReply_UnitId: return "Invalid Device Reply - Unit ID";
    case ModbusError::InvalidDeviceReply_ReplyLength: return "Invalid Device Reply - Reply Length";
    default: return "Unknown";
    }
}
