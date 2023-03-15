#pragma once

#include <cstddef>
#include <cstdint>

namespace hex
{
    /// \brief  An algorithm that can encrypt or decrypt blocks of data.
    class ICryptographyAlgorithm
    {
    public:
        virtual ~ICryptographyAlgorithm() = default;

        /// \brief  Gets the algorithm name.
        virtual const char* Name() const = 0;

        /// \brief  Gets the block length in bits, or 0 for stream cyphers.
        virtual int BlockLength() const = 0;

        /// \brief  Gets the key length in bits.
        virtual int KeyLength() const = 0;

        /// \brief  Gets a value indicating if this algorithm requires a password.
        virtual bool NeedsPassword() const = 0;

        /// \brief  Sets the password for this algorithm.
        ///
        /// \param  password  The password. If null, any previously set password is cleared.
        virtual void SetPassword(const char* password) = 0;


        /// \brief  Gets the encrypted size of a block of the given length.
        ///
        /// \param  length      The length of the data to be encrypted.
        /// \param  finalBlock  True if this is the last block to be encrypted, or false if not.
        /// 
        /// \exception  cryptography_error  An error occurred computing the encrypted size.
        virtual std::size_t EncryptedSize(
            std::size_t length,
            bool finalBlock = true) = 0;

        /// \brief  Encrypts a block of bytes.
        ///
        /// \param  buffer      The buffer to encrypt.
        /// \param  dataLen     The length of the data to encrypt.
        /// \param  bufferLen   The length of \p buffer.
        /// \param  finalBlock  True if this is the last block to be encrypted, or false if not.
        /// 
        /// \returns  The size in bytes of the encrypted data.
        /// 
        /// \exception  cryptography_error  An error occurred encrypting the memory.
        virtual std::size_t Encrypt(
            std::uint8_t* buffer,
            std::size_t dataLen,
            std::size_t bufferLen,
            bool finalBlock = true) = 0;

        /// \brief  Decrypts a block of bytes.
        ///
        /// \param  buffer      The buffer to decrypt.
        /// \param  dataLen     The length of the data to decrypt.
        /// \param  finalBlock  True if this is the last block to be encrypted, or false if not.
        /// 
        /// \returns  The size in bytes of the decrypted data.
        /// 
        /// \exception  cryptography_error  An error occurred decrypting the memory.
        virtual std::size_t Decrypt(
            std::uint8_t* buffer,
            std::size_t dataLen,
            bool finalBlock = true) = 0;
    };
}
